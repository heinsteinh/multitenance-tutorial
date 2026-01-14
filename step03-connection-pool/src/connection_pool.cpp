#include "pool/connection_pool.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace pool {

// ==================== PooledConnection Implementation ====================

PooledConnection::PooledConnection(std::unique_ptr<db::Database> conn, ReleaseFunc release)
    : conn_(std::move(conn))
    , release_func_(std::move(release))
{
}

PooledConnection::~PooledConnection() {
    release();
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : conn_(std::move(other.conn_))
    , release_func_(std::move(other.release_func_))
{
    other.release_func_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        release();
        conn_ = std::move(other.conn_);
        release_func_ = std::move(other.release_func_);
        other.release_func_ = nullptr;
    }
    return *this;
}

void PooledConnection::release() {
    if (conn_ && release_func_) {
        release_func_(std::move(conn_));
        release_func_ = nullptr;
    }
    conn_ = nullptr;
}

// ==================== ConnectionPool Implementation ====================

ConnectionPool::ConnectionPool(const PoolConfig& config)
    : config_(config)
{
    config_.validate();
    spdlog::info("Creating connection pool for '{}' (min={}, max={})",
                 config_.db_path, config_.min_connections, config_.max_connections);
    
    warm_pool();
}

ConnectionPool::ConnectionPool(const std::string& db_path, size_t max_connections)
    : ConnectionPool(PoolConfig{
        .db_path = db_path,
        .min_connections = 1,
        .max_connections = max_connections
    })
{
}

ConnectionPool::~ConnectionPool() {
    spdlog::debug("Shutting down connection pool");
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        pool_.clear();
    }
    
    available_cv_.notify_all();
    
    spdlog::info("Connection pool shutdown complete. Stats: {} created, {} acquisitions, {} timeouts",
                 total_created_.load(), total_acquisitions_.load(), timeouts_.load());
}

void ConnectionPool::warm_pool() {
    spdlog::debug("Warming pool with {} connections", config_.min_connections);
    
    for (size_t i = 0; i < config_.min_connections; ++i) {
        try {
            auto conn = create_connection();
            pool_.push_back(std::move(conn));
        } catch (const std::exception& e) {
            spdlog::error("Failed to create warm-up connection {}: {}", i, e.what());
            throw;
        }
    }
}

std::unique_ptr<db::Database> ConnectionPool::create_connection() {
    db::DatabaseConfig db_config{
        .path = config_.db_path,
        .create_if_missing = config_.create_if_missing,
        .read_only = false,
        .busy_timeout_ms = config_.busy_timeout_ms,
        .enable_foreign_keys = config_.enable_foreign_keys,
        .enable_wal_mode = config_.enable_wal_mode,
        .synchronous = config_.synchronous
    };
    
    auto conn = std::make_unique<db::Database>(db_config);
    ++total_created_;
    
    spdlog::trace("Created new connection (total: {})", total_created_.load());
    return conn;
}

PooledConnection ConnectionPool::acquire() {
    auto start = std::chrono::steady_clock::now();
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (shutdown_) {
        throw std::runtime_error("Connection pool is shut down");
    }
    
    ++waiting_count_;
    
    // Wait for available connection or room to create new one
    bool success = available_cv_.wait_for(lock, config_.acquire_timeout, [this] {
        return shutdown_ || 
               !pool_.empty() || 
               (active_count_.load() < config_.max_connections);
    });
    
    --waiting_count_;
    
    if (shutdown_) {
        throw std::runtime_error("Connection pool is shut down");
    }
    
    if (!success) {
        ++timeouts_;
        throw std::runtime_error(fmt::format(
            "Timeout acquiring connection after {}ms (active={}, max={})",
            config_.acquire_timeout.count(),
            active_count_.load(),
            config_.max_connections
        ));
    }
    
    std::unique_ptr<db::Database> conn;
    
    if (!pool_.empty()) {
        // Get existing connection from pool
        conn = std::move(pool_.front());
        pool_.pop_front();
        
        // Validate the connection
        if (!validate_connection(*conn)) {
            spdlog::warn("Connection failed health check, creating new one");
            ++failed_health_checks_;
            conn = create_connection();
        }
    } else {
        // Create new connection
        conn = create_connection();
    }
    
    ++active_count_;
    ++total_acquisitions_;
    update_peak();
    
    // Track timing
    auto elapsed = std::chrono::steady_clock::now() - start;
    double elapsed_us = std::chrono::duration<double, std::micro>(elapsed).count();
    
    // Update average (simple moving calculation)
    double total = total_acquire_time_us_.load() + elapsed_us;
    total_acquire_time_us_.store(total);
    
    if (elapsed_us > max_acquire_time_us_.load()) {
        max_acquire_time_us_.store(elapsed_us);
    }
    
    spdlog::trace("Acquired connection (active={}, available={})", 
                  active_count_.load(), pool_.size());
    
    // Create release function that captures 'this'
    auto release_func = [this](std::unique_ptr<db::Database> c) {
        this->release(std::move(c));
    };
    
    return PooledConnection(std::move(conn), release_func);
}

std::optional<PooledConnection> ConnectionPool::try_acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutdown_) {
        return std::nullopt;
    }
    
    std::unique_ptr<db::Database> conn;
    
    if (!pool_.empty()) {
        conn = std::move(pool_.front());
        pool_.pop_front();
        
        if (!validate_connection(*conn)) {
            ++failed_health_checks_;
            // Try to create new connection if under limit
            if (active_count_.load() < config_.max_connections) {
                try {
                    conn = create_connection();
                } catch (...) {
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        }
    } else if (active_count_.load() < config_.max_connections) {
        try {
            conn = create_connection();
        } catch (...) {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
    
    ++active_count_;
    ++total_acquisitions_;
    update_peak();
    
    auto release_func = [this](std::unique_ptr<db::Database> c) {
        this->release(std::move(c));
    };
    
    return PooledConnection(std::move(conn), release_func);
}

void ConnectionPool::release(std::unique_ptr<db::Database> conn) {
    if (!conn) return;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        --active_count_;
        ++total_releases_;
        
        if (!shutdown_) {
            pool_.push_back(std::move(conn));
            spdlog::trace("Released connection (active={}, available={})",
                          active_count_.load(), pool_.size());
        }
    }
    
    available_cv_.notify_one();
}

bool ConnectionPool::validate_connection(db::Database& conn) {
    try {
        // Simple health check - execute a trivial query
        auto result = conn.query_single<int>("SELECT 1");
        return result.has_value() && result.value() == 1;
    } catch (const std::exception& e) {
        spdlog::warn("Connection validation failed: {}", e.what());
        return false;
    }
}

PoolStats ConnectionPool::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t acquisitions = total_acquisitions_.load();
    double avg_time = acquisitions > 0 
        ? total_acquire_time_us_.load() / acquisitions 
        : 0.0;
    
    return PoolStats{
        .total_connections = total_created_.load(),
        .active_connections = active_count_.load(),
        .available_connections = pool_.size(),
        .waiting_threads = waiting_count_.load(),
        .peak_connections = peak_active_.load(),
        .total_acquisitions = acquisitions,
        .total_releases = total_releases_.load(),
        .timeouts = timeouts_.load(),
        .failed_health_checks = failed_health_checks_.load(),
        .avg_acquire_time_us = avg_time,
        .max_acquire_time_us = max_acquire_time_us_.load()
    };
}

void ConnectionPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.clear();
    spdlog::debug("Cleared all idle connections from pool");
}

bool ConnectionPool::is_healthy() const {
    try {
        // Try to create a test connection
        db::DatabaseConfig db_config{
            .path = config_.db_path,
            .create_if_missing = false,
            .read_only = true
        };
        db::Database test_conn(db_config);
        return test_conn.query_single<int>("SELECT 1").value_or(0) == 1;
    } catch (...) {
        return false;
    }
}

size_t ConnectionPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

size_t ConnectionPool::active() const {
    return active_count_.load();
}

void ConnectionPool::update_peak() {
    size_t current = active_count_.load();
    size_t peak = peak_active_.load();
    while (current > peak && !peak_active_.compare_exchange_weak(peak, current)) {
        // Retry until successful or current is no longer greater
    }
}

} // namespace pool
