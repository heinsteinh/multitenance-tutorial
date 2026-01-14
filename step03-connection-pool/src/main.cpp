/**
 * Step 03: Connection Pool Demo
 *
 * Demonstrates:
 * - Pool creation and configuration
 * - Connection acquisition and release
 * - Concurrent access from multiple threads
 * - Pool statistics monitoring
 */

#include "pool/connection_pool.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <random>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace std::chrono_literals;

void setup_logging() {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
}

void print_stats(const pool::PoolStats &stats) {
  spdlog::info("Pool Stats:");
  spdlog::info("  Total created:    {}", stats.total_connections);
  spdlog::info("  Active:           {}", stats.active_connections);
  spdlog::info("  Available:        {}", stats.available_connections);
  spdlog::info("  Peak active:      {}", stats.peak_connections);
  spdlog::info("  Total acquires:   {}", stats.total_acquisitions);
  spdlog::info("  Total releases:   {}", stats.total_releases);
  spdlog::info("  Timeouts:         {}", stats.timeouts);
  spdlog::info("  Health failures:  {}", stats.failed_health_checks);
  spdlog::info("  Avg acquire time: {:.2f} µs", stats.avg_acquire_time_us);
  spdlog::info("  Max acquire time: {:.2f} µs", stats.max_acquire_time_us);
}

void demo_basic_usage(pool::ConnectionPool &pool) {
  spdlog::info("=== Basic Usage ===");

  // Acquire and use a connection
  {
    auto conn = pool.acquire();
    spdlog::info("Acquired connection, executing query...");

    conn->execute(R"(
            CREATE TABLE IF NOT EXISTS demo (
                id INTEGER PRIMARY KEY,
                value TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )");

    conn->execute("INSERT INTO demo (value) VALUES ('test1')");
    conn->execute("INSERT INTO demo (value) VALUES ('test2')");

    auto count = conn->query_single<int>("SELECT COUNT(*) FROM demo");
    spdlog::info("Rows in demo table: {}", count.value_or(0));

    // Connection automatically returned to pool when scope ends
  }

  spdlog::info("Connection returned to pool");
  print_stats(pool.stats());
}

void demo_try_acquire(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== Try Acquire (Non-blocking) ===");

  // Non-blocking acquire
  if (auto conn = pool.try_acquire(); conn) {
    spdlog::info("Got connection via try_acquire");
    (*conn)->execute("INSERT INTO demo (value) VALUES ('from try_acquire')");
  } else {
    spdlog::info("No connection available (would have blocked)");
  }
}

void demo_concurrent_access(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== Concurrent Access ===");

  constexpr int num_threads = 10;
  constexpr int operations_per_thread = 50;

  std::vector<std::thread> threads;
  std::atomic<int> successful_ops{0};
  std::atomic<int> failed_ops{0};

  auto worker = [&](int thread_id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sleep_dist(1, 10);

    for (int i = 0; i < operations_per_thread; ++i) {
      try {
        auto conn = pool.acquire();

        // Simulate some work
        conn->execute("SELECT COUNT(*) FROM demo");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(gen)));

        ++successful_ops;
      } catch (const std::exception &e) {
        spdlog::warn("Thread {} operation {} failed: {}", thread_id, i,
                     e.what());
        ++failed_ops;
      }
    }
  };

  auto start = std::chrono::steady_clock::now();

  // Start worker threads
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker, i);
  }

  // Wait for completion
  for (auto &t : threads) {
    t.join();
  }

  auto elapsed = std::chrono::steady_clock::now() - start;
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  spdlog::info("Concurrent test complete:");
  spdlog::info("  Threads:       {}", num_threads);
  spdlog::info("  Ops/thread:    {}", operations_per_thread);
  spdlog::info("  Successful:    {}", successful_ops.load());
  spdlog::info("  Failed:        {}", failed_ops.load());
  spdlog::info("  Time:          {} ms", ms);
  spdlog::info("  Ops/sec:       {:.0f}", successful_ops.load() * 1000.0 / ms);

  print_stats(pool.stats());
}

void demo_pool_exhaustion(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== Pool Exhaustion ===");

  // Acquire all connections and hold them
  std::vector<pool::PooledConnection> held_connections;

  spdlog::info("Acquiring all available connections...");

  while (auto conn = pool.try_acquire()) {
    held_connections.push_back(std::move(*conn));
    spdlog::info("  Held {} connections", held_connections.size());
  }

  spdlog::info("Pool exhausted, {} connections held", held_connections.size());

  // Try to acquire one more (should fail quickly)
  auto start = std::chrono::steady_clock::now();
  try {
    // This will timeout
    auto conn = pool.acquire();
    spdlog::error("Unexpectedly got a connection");
  } catch (const std::exception &e) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    spdlog::info("Timeout as expected after {} ms: {}", ms, e.what());
  }

  // Release all held connections
  spdlog::info("Releasing held connections...");
  held_connections.clear();

  print_stats(pool.stats());
}

void demo_early_release() {
  spdlog::info("");
  spdlog::info("=== Early Release ===");

  // Create a small pool to demonstrate
  pool::ConnectionPool small_pool(
      pool::PoolConfig{.db_path = ":memory:",
                       .min_connections = 1,
                       .max_connections = 2,
                       .acquire_timeout = std::chrono::milliseconds(100)});

  {
    auto conn = small_pool.acquire();
    spdlog::info("Active before early release: {}", small_pool.active());

    // Do some work
    conn->execute("SELECT 1");

    // Release early (before scope ends)
    conn.release();
    spdlog::info("Active after early release: {}", small_pool.active());

    // Connection is now invalid
    // conn->execute("SELECT 1");  // Would crash!
  }
}

int main() {
  setup_logging();

  spdlog::info("╔════════════════════════════════════════════╗");
  spdlog::info("║  Step 03: Connection Pool Demo             ║");
  spdlog::info("╚════════════════════════════════════════════╝");
  spdlog::info("");

  // Clean up any existing test database
  std::filesystem::remove("step03_demo.db");
  std::filesystem::remove("step03_demo.db-wal");
  std::filesystem::remove("step03_demo.db-shm");

  try {
    // Create pool with configuration
    pool::PoolConfig config{.db_path = "step03_demo.db",
                            .min_connections = 2,
                            .max_connections = 5,
                            .acquire_timeout = std::chrono::milliseconds(1000),
                            .enable_foreign_keys = true,
                            .enable_wal_mode = true};

    pool::ConnectionPool pool(config);

    spdlog::info("Pool created with {} min, {} max connections",
                 config.min_connections, config.max_connections);
    spdlog::info("");

    demo_basic_usage(pool);
    demo_try_acquire(pool);
    demo_concurrent_access(pool);
    demo_pool_exhaustion(pool);
    demo_early_release();

    spdlog::info("");
    spdlog::info("=== Final Pool Stats ===");
    print_stats(pool.stats());

    spdlog::info("");
    spdlog::info("=== Demo Complete ===");
    spdlog::info("Next: Step 04 - Repository Pattern");

  } catch (const std::exception &e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }

  // Cleanup
  std::filesystem::remove("step03_demo.db");
  std::filesystem::remove("step03_demo.db-wal");
  std::filesystem::remove("step03_demo.db-shm");

  return 0;
}
