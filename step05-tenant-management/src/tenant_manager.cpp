#include "tenant/tenant_manager.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace tenant {

TenantManager::TenantManager(const TenantManagerConfig& config)
    : config_(config)
{
    // Create tenant database directory if needed
    fs::create_directories(config_.tenant_db_directory);

    // Initialize system database pool
    pool::PoolConfig system_pool_config{
        .db_path = config_.system_db_path,
        .min_connections = 2,
        .max_connections = 10,
        .enable_foreign_keys = true,
        .enable_wal_mode = config_.enable_wal_mode
    };

    system_pool_ = std::make_unique<pool::ConnectionPool>(system_pool_config);

    // Initialize system schema
    init_system_schema();

    spdlog::info("TenantManager initialized: system_db={}, tenant_dir={}",
                 config_.system_db_path, config_.tenant_db_directory);
}

TenantManager::TenantManager(const std::string& system_db_path,
                             const std::string& tenant_db_directory)
    : TenantManager(TenantManagerConfig{
        .system_db_path = system_db_path,
        .tenant_db_directory = tenant_db_directory
    })
{
}

TenantManager::~TenantManager() {
    close_all_pools();
}

void TenantManager::init_system_schema() {
    auto conn = system_pool_->acquire();

    conn->execute(R"(
        CREATE TABLE IF NOT EXISTS tenants (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            plan TEXT DEFAULT 'free',
            active INTEGER DEFAULT 1,
            db_path TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");

    conn->execute(R"(
        CREATE TABLE IF NOT EXISTS system_users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT,
            role TEXT DEFAULT 'admin',
            active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");

    conn->execute("CREATE INDEX IF NOT EXISTS idx_tenants_active ON tenants(active)");

    spdlog::debug("System schema initialized");
}

pool::ConnectionPool& TenantManager::get_pool(const std::string& tenant_id) {
    std::lock_guard<std::mutex> lock(pools_mutex_);

    auto it = tenant_pools_.find(tenant_id);
    if (it != tenant_pools_.end()) {
        return *it->second;
    }

    // Check if tenant exists in system database
    if (!is_tenant_active(tenant_id)) {
        throw std::runtime_error(fmt::format("Tenant '{}' not found or inactive", tenant_id));
    }

    // Create pool
    auto pool = create_tenant_pool(tenant_id);
    auto& ref = *pool;
    tenant_pools_[tenant_id] = std::move(pool);

    spdlog::debug("Created connection pool for tenant '{}'", tenant_id);
    return ref;
}

pool::ConnectionPool& TenantManager::get_current_pool() {
    return get_pool(TenantContext::tenant_id());
}

pool::ConnectionPool& TenantManager::get_system_pool() {
    return *system_pool_;
}

std::unique_ptr<pool::ConnectionPool> TenantManager::create_tenant_pool(const std::string& tenant_id) {
    pool::PoolConfig pool_config{
        .db_path = get_tenant_db_path(tenant_id),
        .min_connections = config_.pool_min_connections,
        .max_connections = config_.pool_max_connections,
        .enable_foreign_keys = config_.enable_foreign_keys,
        .enable_wal_mode = config_.enable_wal_mode
    };

    return std::make_unique<pool::ConnectionPool>(pool_config);
}

void TenantManager::provision_tenant(const repository::Tenant& tenant) {
    spdlog::info("Provisioning tenant: {}", tenant.tenant_id);

    std::string db_path = get_tenant_db_path(tenant.tenant_id);

    // Check if already exists
    if (fs::exists(db_path)) {
        throw std::runtime_error(fmt::format("Tenant database already exists: {}", db_path));
    }

    // Create tenant database
    db::Database tenant_db(db::DatabaseConfig{
        .path = db_path,
        .create_if_missing = true,
        .enable_foreign_keys = config_.enable_foreign_keys,
        .enable_wal_mode = config_.enable_wal_mode
    });

    // Run tenant schema
    run_tenant_schema(tenant_db);

    // Register in system database
    {
        auto conn = system_pool_->acquire();
        auto stmt = conn->prepare(R"(
            INSERT INTO tenants (tenant_id, name, plan, active, db_path)
            VALUES (?, ?, ?, ?, ?)
        )");

        stmt.bind(1, tenant.tenant_id);
        stmt.bind(2, tenant.name);
        stmt.bind(3, tenant.plan);
        stmt.bind(4, tenant.active ? 1 : 0);
        stmt.bind(5, db_path);
        stmt.step();
    }

    spdlog::info("Tenant '{}' provisioned successfully", tenant.tenant_id);
}

void TenantManager::run_tenant_schema(db::Database& db) {
    // Standard tenant schema
    db.execute(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT NOT NULL,
            password_hash TEXT,
            role TEXT DEFAULT 'user',
            active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS products (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT,
            price REAL NOT NULL DEFAULT 0,
            stock INTEGER DEFAULT 0,
            active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS orders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            status TEXT DEFAULT 'pending',
            total REAL DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    )");

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS order_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            order_id INTEGER NOT NULL,
            product_id INTEGER NOT NULL,
            quantity INTEGER DEFAULT 1,
            price REAL NOT NULL,
            FOREIGN KEY (order_id) REFERENCES orders(id),
            FOREIGN KEY (product_id) REFERENCES products(id)
        )
    )");

    db.execute("CREATE INDEX IF NOT EXISTS idx_orders_user ON orders(user_id)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_order_items_order ON order_items(order_id)");

    spdlog::debug("Tenant schema created");
}

void TenantManager::deprovision_tenant(const std::string& tenant_id, bool delete_data) {
    spdlog::info("Deprovisioning tenant: {} (delete_data={})", tenant_id, delete_data);

    // Close pool if open
    {
        std::lock_guard<std::mutex> lock(pools_mutex_);
        tenant_pools_.erase(tenant_id);
    }

    // Mark as inactive in system database
    {
        auto conn = system_pool_->acquire();
        auto stmt = conn->prepare("UPDATE tenants SET active = 0, updated_at = datetime('now') WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);
        stmt.step();
    }

    // Optionally delete database file
    if (delete_data) {
        std::string db_path = get_tenant_db_path(tenant_id);
        fs::remove(db_path);
        fs::remove(db_path + "-wal");
        fs::remove(db_path + "-shm");
        spdlog::info("Deleted tenant database: {}", db_path);
    }
}

void TenantManager::suspend_tenant(const std::string& tenant_id) {
    spdlog::info("Suspending tenant: {}", tenant_id);

    // Close pool
    {
        std::lock_guard<std::mutex> lock(pools_mutex_);
        tenant_pools_.erase(tenant_id);
    }

    // Keep active flag but add suspended flag (simplified: just close pool)
}

void TenantManager::resume_tenant(const std::string& tenant_id) {
    spdlog::info("Resuming tenant: {}", tenant_id);
    // Pool will be recreated on next access
}

bool TenantManager::is_tenant_active(const std::string& tenant_id) {
    auto conn = system_pool_->acquire();
    auto stmt = conn->prepare("SELECT active FROM tenants WHERE tenant_id = ?");
    stmt.bind(1, tenant_id);

    if (stmt.step()) {
        return stmt.column<int64_t>(0) != 0;
    }
    return false;
}

void TenantManager::migrate_all_tenants(const MigrationFunc& migration) {
    auto tenant_ids = get_active_tenant_ids();

    spdlog::info("Running migration on {} tenants", tenant_ids.size());

    for (const auto& tenant_id : tenant_ids) {
        try {
            auto& pool = get_pool(tenant_id);
            auto conn = pool.acquire();
            migration(*conn);
            spdlog::debug("Migrated tenant: {}", tenant_id);
        } catch (const std::exception& e) {
            spdlog::error("Migration failed for tenant '{}': {}", tenant_id, e.what());
        }
    }
}

std::vector<std::string> TenantManager::get_active_tenant_ids() {
    std::vector<std::string> ids;

    auto conn = system_pool_->acquire();
    auto stmt = conn->prepare("SELECT tenant_id FROM tenants WHERE active = 1");

    while (stmt.step()) {
        ids.push_back(stmt.column<std::string>(0));
    }

    return ids;
}

std::optional<repository::Tenant> TenantManager::get_tenant(const std::string& tenant_id) {
    auto conn = system_pool_->acquire();
    auto stmt = conn->prepare(R"(
        SELECT id, tenant_id, name, plan, active, db_path, created_at, updated_at
        FROM tenants WHERE tenant_id = ?
    )");
    stmt.bind(1, tenant_id);

    if (stmt.step()) {
        return repository::Tenant{
            .id = stmt.column<int64_t>(0),
            .tenant_id = stmt.column<std::string>(1),
            .name = stmt.column<std::string>(2),
            .plan = stmt.column<std::string>(3),
            .active = stmt.column<int64_t>(4) != 0,
            .db_path = stmt.column<std::string>(5),
            .created_at = stmt.column<std::string>(6),
            .updated_at = stmt.column<std::string>(7)
        };
    }

    return std::nullopt;
}

std::string TenantManager::get_tenant_db_path(const std::string& tenant_id) const {
    return (fs::path(config_.tenant_db_directory) / (tenant_id + ".db")).string();
}

void TenantManager::preload_all_pools() {
    auto tenant_ids = get_active_tenant_ids();

    spdlog::info("Preloading pools for {} tenants", tenant_ids.size());

    for (const auto& tenant_id : tenant_ids) {
        try {
            get_pool(tenant_id);
        } catch (const std::exception& e) {
            spdlog::error("Failed to preload pool for tenant '{}': {}", tenant_id, e.what());
        }
    }
}

void TenantManager::close_all_pools() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    tenant_pools_.clear();
    spdlog::debug("Closed all tenant pools");
}

TenantManager::ManagerStats TenantManager::stats() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);

    ManagerStats s{};
    s.active_pools = tenant_pools_.size();

    for (const auto& [id, pool] : tenant_pools_) {
        auto pool_stats = pool->stats();
        s.total_connections += pool_stats.total_connections;
        s.active_connections += pool_stats.active_connections;
    }

    // Get total tenants from system DB
    auto conn = system_pool_->acquire();
    auto result = conn->query_single<int>("SELECT COUNT(*) FROM tenants WHERE active = 1");
    s.total_tenants = result.value_or(0);

    return s;
}

} // namespace tenant
