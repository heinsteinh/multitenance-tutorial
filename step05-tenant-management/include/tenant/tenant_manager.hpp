#pragma once

#include "tenant/tenant_context.hpp"
#include "pool/connection_pool.hpp"
#include "repository/user_repository.hpp"

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <filesystem>

namespace tenant {

/**
 * Configuration for the tenant manager
 */
struct TenantManagerConfig {
    std::string system_db_path;          // Path to system database
    std::string tenant_db_directory;     // Directory for tenant databases

    // Pool settings per tenant
    size_t pool_min_connections = 1;
    size_t pool_max_connections = 5;

    // Tenant database settings
    bool enable_wal_mode = true;
    bool enable_foreign_keys = true;
};

/**
 * Manages tenant databases and connection pools.
 *
 * Provides:
 * - Tenant provisioning and deprovisioning
 * - Connection pool management per tenant
 * - Schema migration across tenants
 * - Tenant lifecycle operations
 */
class TenantManager {
public:
    /**
     * Create a tenant manager
     * @param config Configuration options
     */
    explicit TenantManager(const TenantManagerConfig& config);

    /**
     * Simple constructor with defaults
     */
    TenantManager(const std::string& system_db_path,
                  const std::string& tenant_db_directory);

    ~TenantManager();

    // Non-copyable, non-movable
    TenantManager(const TenantManager&) = delete;
    TenantManager& operator=(const TenantManager&) = delete;

    // ==================== Pool Access ====================

    /**
     * Get connection pool for a tenant
     * Creates the pool if it doesn't exist
     * @throws std::runtime_error if tenant doesn't exist
     */
    pool::ConnectionPool& get_pool(const std::string& tenant_id);

    /**
     * Get connection pool for current context tenant
     * @throws std::runtime_error if no context set
     */
    pool::ConnectionPool& get_current_pool();

    /**
     * Get the system database pool
     */
    pool::ConnectionPool& get_system_pool();

    // ==================== Tenant Lifecycle ====================

    /**
     * Provision a new tenant database
     * - Creates the database file
     * - Runs initial schema
     * - Registers in system database
     */
    void provision_tenant(const repository::Tenant& tenant);

    /**
     * Deprovision a tenant
     * - Closes all connections
     * - Optionally deletes database file
     * - Marks as inactive in system database
     */
    void deprovision_tenant(const std::string& tenant_id, bool delete_data = false);

    /**
     * Suspend a tenant (close connections, keep data)
     */
    void suspend_tenant(const std::string& tenant_id);

    /**
     * Resume a suspended tenant
     */
    void resume_tenant(const std::string& tenant_id);

    /**
     * Check if tenant exists and is active
     */
    bool is_tenant_active(const std::string& tenant_id);

    // ==================== Schema Management ====================

    /**
     * Run a migration function on all tenant databases
     */
    using MigrationFunc = std::function<void(db::Database&)>;
    void migrate_all_tenants(const MigrationFunc& migration);

    /**
     * Run initial schema on a tenant database
     */
    void run_tenant_schema(db::Database& db);

    // ==================== Utility ====================

    /**
     * Get list of all active tenant IDs
     */
    std::vector<std::string> get_active_tenant_ids();

    /**
     * Get tenant info from system database
     */
    std::optional<repository::Tenant> get_tenant(const std::string& tenant_id);

    /**
     * Get database path for a tenant
     */
    std::string get_tenant_db_path(const std::string& tenant_id) const;

    /**
     * Preload pools for all active tenants
     */
    void preload_all_pools();

    /**
     * Close all tenant pools (for shutdown)
     */
    void close_all_pools();

    /**
     * Get statistics about managed pools
     */
    struct ManagerStats {
        size_t total_tenants;
        size_t active_pools;
        size_t total_connections;
        size_t active_connections;
    };
    ManagerStats stats() const;

private:
    TenantManagerConfig config_;

    // System database pool
    std::unique_ptr<pool::ConnectionPool> system_pool_;

    // Tenant pools (created on demand)
    mutable std::mutex pools_mutex_;
    std::unordered_map<std::string, std::unique_ptr<pool::ConnectionPool>> tenant_pools_;

    // Initialize system database schema
    void init_system_schema();

    // Create pool for tenant
    std::unique_ptr<pool::ConnectionPool> create_tenant_pool(const std::string& tenant_id);
};

} // namespace tenant
