#include "db/schema_initializer.hpp"

#include <spdlog/spdlog.h>

namespace db
{

    SchemaInitializer::SchemaInitializer(std::shared_ptr<Database> database)
        : database_(std::move(database))
    {
    }

    void SchemaInitializer::initialize_all()
    {
        auto tx = database_->transaction();

        create_schema_version_table();

        int current = get_current_version();
        if(current < SCHEMA_VERSION)
        {
            initialize_tenants_table();
            initialize_users_table();
            initialize_roles_tables();
            set_version(SCHEMA_VERSION);
        }

        tx.commit();
        spdlog::info("Database schema initialized (version {})", SCHEMA_VERSION);
    }

    void SchemaInitializer::create_schema_version_table()
    {
        if(!database_->table_exists("schema_version"))
        {
            database_->execute(R"(
                CREATE TABLE schema_version (
                    version INTEGER PRIMARY KEY,
                    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            )");
            database_->execute("INSERT INTO schema_version (version) VALUES (0)");
            spdlog::info("Created schema_version table");
        }
    }

    int SchemaInitializer::get_current_version()
    {
        auto result = database_->query_single<int>(
            "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1");
        return result.value_or(0);
    }

    void SchemaInitializer::set_version(int version)
    {
        auto stmt =
            database_->prepare("UPDATE schema_version SET version = ?");
        stmt.bind(1, version);
        stmt.step();
    }

    void SchemaInitializer::initialize_users_table()
    {
        if(!database_->table_exists("users"))
        {
            database_->execute(R"(
                CREATE TABLE users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    tenant_id TEXT NOT NULL,
                    username TEXT NOT NULL,
                    email TEXT NOT NULL UNIQUE,
                    password_hash TEXT,
                    role TEXT DEFAULT 'user',
                    active INTEGER DEFAULT 1,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(tenant_id, username)
                )
            )");

            database_->execute(
                "CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id)");
            database_->execute(
                "CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)");

            spdlog::info("Created users table");
        }
    }

    void SchemaInitializer::initialize_tenants_table()
    {
        if(!database_->table_exists("tenants"))
        {
            database_->execute(R"(
                CREATE TABLE tenants (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    tenant_id TEXT NOT NULL UNIQUE,
                    name TEXT NOT NULL,
                    plan TEXT DEFAULT 'basic',
                    active INTEGER DEFAULT 1,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            )");

            database_->execute(
                "CREATE INDEX IF NOT EXISTS idx_tenants_tenant_id ON tenants(tenant_id)");
            database_->execute(
                "CREATE INDEX IF NOT EXISTS idx_tenants_active ON tenants(active)");

            spdlog::info("Created tenants table");
        }
    }

    void SchemaInitializer::initialize_roles_tables()
    {
        if(!database_->table_exists("roles"))
        {
            database_->execute(R"(
                CREATE TABLE roles (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    tenant_id TEXT NOT NULL,
                    name TEXT NOT NULL,
                    parent_role TEXT,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(tenant_id, name)
                )
            )");
            spdlog::info("Created roles table");
        }

        if(!database_->table_exists("role_permissions"))
        {
            database_->execute(R"(
                CREATE TABLE role_permissions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    role_id INTEGER NOT NULL,
                    resource TEXT NOT NULL,
                    action TEXT NOT NULL,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY(role_id) REFERENCES roles(id),
                    UNIQUE(role_id, resource, action)
                )
            )");
            spdlog::info("Created role_permissions table");
        }

        if(!database_->table_exists("user_roles"))
        {
            database_->execute(R"(
                CREATE TABLE user_roles (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id INTEGER NOT NULL,
                    role_id INTEGER NOT NULL,
                    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(user_id, role_id)
                )
            )");
            spdlog::info("Created user_roles table");
        }
    }

    void SchemaInitializer::seed_default_data()
    {
        // Seed demo tenant if not exists
        auto tenant_exists = database_->query_single<int>(
            "SELECT COUNT(*) FROM tenants WHERE tenant_id = 'demo'");

        if(!tenant_exists || tenant_exists.value() == 0)
        {
            database_->execute(R"(
                INSERT INTO tenants (tenant_id, name, plan, active)
                VALUES ('demo', 'Demo Tenant', 'enterprise', 1)
            )");
            spdlog::info("Created demo tenant");
        }

        // Seed default roles if not exists
        auto admin_exists = database_->query_single<int>(
            "SELECT COUNT(*) FROM roles WHERE name = 'admin'");

        if(!admin_exists || admin_exists.value() == 0)
        {
            // Create admin role
            database_->execute(R"(
                INSERT INTO roles (tenant_id, name) VALUES ('default', 'admin')
            )");

            // Get admin role ID
            auto admin_id = database_->query_single<int64_t>(
                "SELECT id FROM roles WHERE name = 'admin'");

            if(admin_id)
            {
                // Add admin permissions
                auto stmt = database_->prepare(
                    "INSERT INTO role_permissions (role_id, resource, action) VALUES (?, ?, ?)");

                for(const auto& resource : {"users", "tenants", "roles"})
                {
                    for(const auto& action : {"create", "read", "update", "delete"})
                    {
                        stmt.bind(1, admin_id.value());
                        stmt.bind(2, resource);
                        stmt.bind(3, action);
                        stmt.step();
                        stmt.reset();
                        stmt.clear_bindings();
                    }
                }
            }

            spdlog::info("Created admin role with full permissions");
        }

        // Create user role
        auto user_exists = database_->query_single<int>(
            "SELECT COUNT(*) FROM roles WHERE name = 'user'");

        if(!user_exists || user_exists.value() == 0)
        {
            database_->execute(R"(
                INSERT INTO roles (tenant_id, name) VALUES ('default', 'user')
            )");

            auto user_id = database_->query_single<int64_t>(
                "SELECT id FROM roles WHERE name = 'user'");

            if(user_id)
            {
                auto stmt = database_->prepare(
                    "INSERT INTO role_permissions (role_id, resource, action) VALUES (?, ?, ?)");

                // Users can read users and tenants
                stmt.bind(1, user_id.value());
                stmt.bind(2, "users");
                stmt.bind(3, "read");
                stmt.step();
                stmt.reset();
                stmt.clear_bindings();

                stmt.bind(1, user_id.value());
                stmt.bind(2, "tenants");
                stmt.bind(3, "read");
                stmt.step();
            }

            spdlog::info("Created user role with read permissions");
        }

        spdlog::info("Default data seeded successfully");
    }

} // namespace db
