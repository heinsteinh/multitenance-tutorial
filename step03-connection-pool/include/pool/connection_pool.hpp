#pragma once

#include "pool/pool_config.hpp"
#include "pool/pooled_connection.hpp"
#include "db/database.hpp"

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <chrono>
#include <optional>
#include <thread>

namespace pool {

/**
 * Thread-safe connection pool for SQLite databases.
 * 
 * Features:
 * - Pre-warms connections on startup
 * - Bounds maximum concurrent connections
 * - Blocks callers when pool is exhausted (with timeout)
 * - Automatically returns connections when PooledConnection is destroyed
 * - Health checks and connection validation
 * 
 * Usage:
 *   ConnectionPool pool(PoolConfig{.db_path = "app.db", .max_connections = 10});
 *   
 *   // In any thread:
 *   {
 *       auto conn = pool.acquire();
 *       conn->execute("SELECT ...");
 *   }  // Connection returned automatically
 */
class ConnectionPool {
public:
    /**
     * Create a connection pool
     * @param config Pool configuration
     */
    explicit ConnectionPool(const PoolConfig& config);
    
    /**
     * Convenience constructor
     */
    explicit ConnectionPool(const std::string& db_path, size_t max_connections = 10);
    
    ~ConnectionPool();

    // Non-copyable, non-movable (contains mutex)
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    /**
     * Acquire a connection from the pool.
     * Blocks if no connections available until timeout.
     * 
     * @return PooledConnection that auto-releases on destruction
     * @throws std::runtime_error if timeout exceeded
     */
    PooledConnection acquire();

    /**
     * Try to acquire a connection without blocking.
     * 
     * @return Optional containing connection, or nullopt if none available
     */
    std::optional<PooledConnection> try_acquire();

    /**
     * Get current pool statistics
     */
    PoolStats stats() const;

    /**
     * Get pool configuration
     */
    const PoolConfig& config() const { return config_; }

    /**
     * Close all idle connections and reset counters
     */
    void clear();

    /**
     * Check if pool is healthy
     * @return true if at least one connection can be established
     */
    bool is_healthy() const;

    /**
     * Get current number of available connections
     */
    size_t available() const;

    /**
     * Get current number of active (checked out) connections
     */
    size_t active() const;

private:
    PoolConfig config_;
    
    mutable std::mutex mutex_;
    std::condition_variable available_cv_;
    
    // Available connections (not in use)
    std::deque<std::unique_ptr<db::Database>> pool_;
    
    // Statistics (some atomic for lock-free reads)
    std::atomic<size_t> total_created_{0};
    std::atomic<size_t> active_count_{0};
    std::atomic<size_t> waiting_count_{0};
    std::atomic<size_t> total_acquisitions_{0};
    std::atomic<size_t> total_releases_{0};
    std::atomic<size_t> timeouts_{0};
    std::atomic<size_t> failed_health_checks_{0};
    std::atomic<size_t> peak_active_{0};
    
    std::atomic<double> total_acquire_time_us_{0};
    std::atomic<double> max_acquire_time_us_{0};
    
    bool shutdown_{false};

    // Create a new database connection
    std::unique_ptr<db::Database> create_connection();
    
    // Release a connection back to the pool
    void release(std::unique_ptr<db::Database> conn);
    
    // Check if a connection is still valid
    bool validate_connection(db::Database& conn);
    
    // Pre-warm the pool with min_connections
    void warm_pool();
    
    // Update peak active tracking
    void update_peak();
};

} // namespace pool
