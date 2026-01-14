/**
 * Step 03: Connection Pool Tests
 */

#include <catch2/catch_test_macros.hpp>
#include "pool/connection_pool.hpp"

#include <thread>
#include <vector>
#include <chrono>

using namespace pool;
using namespace std::chrono_literals;

TEST_CASE("Pool can be created", "[pool]") {
    SECTION("With config") {
        PoolConfig config{
            .db_path = ":memory:",
            .min_connections = 2,
            .max_connections = 5
        };
        ConnectionPool pool(config);

        auto stats = pool.stats();
        REQUIRE(stats.available_connections >= 2);
    }

    SECTION("With path only") {
        ConnectionPool pool(":memory:", 5);
        REQUIRE(pool.config().max_connections == 5);
    }
}

TEST_CASE("Connections can be acquired and released", "[pool]") {
    ConnectionPool pool(":memory:", 3);

    SECTION("Basic acquire/release") {
        {
            auto conn = pool.acquire();
            REQUIRE(conn);
            REQUIRE(pool.active() == 1);
        }
        REQUIRE(pool.active() == 0);
    }

    SECTION("Multiple acquires") {
        auto conn1 = pool.acquire();
        auto conn2 = pool.acquire();

        REQUIRE(pool.active() == 2);

        conn1.release();
        REQUIRE(pool.active() == 1);
    }

    SECTION("try_acquire succeeds when available") {
        auto conn = pool.try_acquire();
        REQUIRE(conn.has_value());
    }
}

TEST_CASE("Pool enforces max connections", "[pool]") {
    PoolConfig config{
        .db_path = ":memory:",
        .min_connections = 1,
        .max_connections = 2,
        .acquire_timeout = 50ms
    };
    ConnectionPool pool(config);

    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();

    REQUIRE(pool.active() == 2);

    SECTION("try_acquire fails when exhausted") {
        auto conn3 = pool.try_acquire();
        REQUIRE_FALSE(conn3.has_value());
    }

    SECTION("acquire times out when exhausted") {
        REQUIRE_THROWS(pool.acquire());

        auto stats = pool.stats();
        REQUIRE(stats.timeouts == 1);
    }
}

TEST_CASE("Pool tracks statistics", "[pool]") {
    ConnectionPool pool(":memory:", 5);

    {
        auto conn = pool.acquire();
    }
    {
        auto conn = pool.acquire();
    }

    auto stats = pool.stats();
    REQUIRE(stats.total_acquisitions == 2);
    REQUIRE(stats.total_releases == 2);
    REQUIRE(stats.active_connections == 0);
}

TEST_CASE("Pool handles concurrent access", "[pool]") {
    ConnectionPool pool(":memory:", 5);

    std::atomic<int> successful{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, &successful]() {
            for (int j = 0; j < 20; ++j) {
                try {
                    auto conn = pool.acquire();
                    conn->execute("SELECT 1");
                    ++successful;
                    std::this_thread::sleep_for(1ms);
                } catch (...) {
                    // Timeout is OK in high contention
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(successful > 0);

    auto stats = pool.stats();
    REQUIRE(stats.peak_connections <= 5);
}

TEST_CASE("Connections execute queries correctly", "[pool]") {
    ConnectionPool pool(":memory:", 2);

    // Create table
    {
        auto conn = pool.acquire();
        conn->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
    }

    // Insert from different connection
    {
        auto conn = pool.acquire();
        conn->execute("INSERT INTO test (value) VALUES ('hello')");
    }

    // Query from another connection
    {
        auto conn = pool.acquire();
        auto result = conn->query_single<std::string>("SELECT value FROM test");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "hello");
    }
}

TEST_CASE("Pool validates connections", "[pool]") {
    ConnectionPool pool(":memory:", 2);

    REQUIRE(pool.is_healthy());
}

TEST_CASE("Pool can be cleared", "[pool]") {
    ConnectionPool pool(":memory:", 5);

    // Acquire and release to populate pool
    {
        auto conn1 = pool.acquire();
        auto conn2 = pool.acquire();
    }

    REQUIRE(pool.available() >= 2);

    pool.clear();

    REQUIRE(pool.available() == 0);
}

TEST_CASE("Early release works", "[pool]") {
    ConnectionPool pool(":memory:", 2);

    auto conn = pool.acquire();
    REQUIRE(pool.active() == 1);

    conn.release();
    REQUIRE(pool.active() == 0);

    // Connection should no longer be valid
    REQUIRE_FALSE(conn);
}

TEST_CASE("Config validation", "[pool]") {
    SECTION("Empty path fails") {
        PoolConfig config{.db_path = ""};
        REQUIRE_THROWS(config.validate());
    }

    SECTION("min > max fails") {
        PoolConfig config{
            .db_path = ":memory:",
            .min_connections = 10,
            .max_connections = 5
        };
        REQUIRE_THROWS(config.validate());
    }

    SECTION("max = 0 fails") {
        PoolConfig config{
            .db_path = ":memory:",
            .max_connections = 0
        };
        REQUIRE_THROWS(config.validate());
    }
}
