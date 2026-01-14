#pragma once

#include "db/database.hpp"
#include <functional>
#include <memory>

namespace pool {

// Forward declaration
class ConnectionPool;

/**
 * RAII wrapper for a pooled database connection.
 * 
 * Automatically returns the connection to the pool when destroyed.
 * Provides pointer-like access to the underlying Database.
 * 
 * Usage:
 *   {
 *       PooledConnection conn = pool.acquire();
 *       conn->execute("SELECT ...");  // Use like a pointer
 *       (*conn).execute("...");       // Or dereference
 *   }  // Connection returned to pool here
 */
class PooledConnection {
public:
    using ReleaseFunc = std::function<void(std::unique_ptr<db::Database>)>;

    /**
     * Create a pooled connection
     * @param conn The database connection
     * @param release Function to call when releasing
     */
    PooledConnection(std::unique_ptr<db::Database> conn, ReleaseFunc release);
    
    ~PooledConnection();

    // Move-only
    PooledConnection(PooledConnection&& other) noexcept;
    PooledConnection& operator=(PooledConnection&& other) noexcept;
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;

    // Pointer-like access
    db::Database* operator->() { return conn_.get(); }
    const db::Database* operator->() const { return conn_.get(); }
    
    db::Database& operator*() { return *conn_; }
    const db::Database& operator*() const { return *conn_; }

    /**
     * Get the underlying database pointer
     */
    db::Database* get() { return conn_.get(); }
    const db::Database* get() const { return conn_.get(); }

    /**
     * Check if connection is valid
     */
    explicit operator bool() const { return conn_ != nullptr; }

    /**
     * Release connection back to pool early
     * After this call, the connection is no longer usable
     */
    void release();

private:
    std::unique_ptr<db::Database> conn_;
    ReleaseFunc release_func_;
};

} // namespace pool
