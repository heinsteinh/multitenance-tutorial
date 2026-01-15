#include "repository/tenant_repository.hpp"

#include <spdlog/spdlog.h>

namespace repository
{

    TenantRepository::TenantRepository(std::shared_ptr<db::Database> database)
        : database_(std::move(database))
    {
    }

    void TenantRepository::initialize_schema()
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

    std::optional<services::TenantModel> TenantRepository::find_by_id(int64_t id)
    {
        auto stmt = database_->prepare(
            "SELECT id, tenant_id, name, plan, active FROM tenants WHERE id = ?");
        stmt.bind(1, id);

        if(stmt.step())
        {
            return map_from_row(stmt);
        }
        return std::nullopt;
    }

    std::optional<services::TenantModel> TenantRepository::find_by_tenant_id(
        const std::string& tenant_id)
    {
        auto stmt = database_->prepare(
            "SELECT id, tenant_id, name, plan, active FROM tenants WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);

        if(stmt.step())
        {
            return map_from_row(stmt);
        }
        return std::nullopt;
    }

    std::vector<services::TenantModel> TenantRepository::find_all()
    {
        std::vector<services::TenantModel> tenants;

        auto stmt = database_->prepare(
            "SELECT id, tenant_id, name, plan, active FROM tenants ORDER BY id");

        while(stmt.step())
        {
            tenants.push_back(map_from_row(stmt));
        }

        return tenants;
    }

    std::vector<services::TenantModel> TenantRepository::find_active()
    {
        std::vector<services::TenantModel> tenants;

        auto stmt = database_->prepare(
            "SELECT id, tenant_id, name, plan, active FROM tenants WHERE active = 1 "
            "ORDER BY id");

        while(stmt.step())
        {
            tenants.push_back(map_from_row(stmt));
        }

        return tenants;
    }

    int64_t TenantRepository::insert(const services::TenantModel& tenant)
    {
        auto stmt = database_->prepare(
            "INSERT INTO tenants (tenant_id, name, plan, active) VALUES (?, ?, ?, ?)");

        stmt.bind(1, tenant.tenant_id);
        stmt.bind(2, tenant.name);
        stmt.bind(3, tenant.plan);
        stmt.bind(4, tenant.active ? 1 : 0);
        stmt.step();

        int64_t id = database_->last_insert_rowid();
        spdlog::debug("Inserted tenant {} with id {}", tenant.tenant_id, id);
        return id;
    }

    void TenantRepository::update(const services::TenantModel& tenant)
    {
        auto stmt = database_->prepare(
            "UPDATE tenants SET tenant_id = ?, name = ?, plan = ?, active = ?, "
            "updated_at = CURRENT_TIMESTAMP WHERE id = ?");

        stmt.bind(1, tenant.tenant_id);
        stmt.bind(2, tenant.name);
        stmt.bind(3, tenant.plan);
        stmt.bind(4, tenant.active ? 1 : 0);
        stmt.bind(5, tenant.id);
        stmt.step();

        spdlog::debug("Updated tenant {}", tenant.id);
    }

    void TenantRepository::remove(int64_t id)
    {
        auto stmt = database_->prepare("DELETE FROM tenants WHERE id = ?");
        stmt.bind(1, id);
        stmt.step();

        spdlog::debug("Deleted tenant {}", id);
    }

    bool TenantRepository::tenant_id_exists(const std::string& tenant_id)
    {
        auto stmt =
            database_->prepare("SELECT COUNT(*) FROM tenants WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);
        stmt.step();
        return stmt.column<int>(0) > 0;
    }

    bool TenantRepository::is_active(const std::string& tenant_id)
    {
        auto stmt = database_->prepare(
            "SELECT active FROM tenants WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);

        if(stmt.step())
        {
            return stmt.column<int>(0) != 0;
        }
        return false;
    }

    void TenantRepository::activate(const std::string& tenant_id)
    {
        auto stmt = database_->prepare(
            "UPDATE tenants SET active = 1, updated_at = CURRENT_TIMESTAMP "
            "WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);
        stmt.step();

        spdlog::info("Activated tenant {}", tenant_id);
    }

    void TenantRepository::deactivate(const std::string& tenant_id)
    {
        auto stmt = database_->prepare(
            "UPDATE tenants SET active = 0, updated_at = CURRENT_TIMESTAMP "
            "WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);
        stmt.step();

        spdlog::info("Deactivated tenant {}", tenant_id);
    }

    services::TenantModel TenantRepository::map_from_row(db::Statement& stmt)
    {
        services::TenantModel tenant;
        tenant.id        = stmt.column<int64_t>(0);
        tenant.tenant_id = stmt.column<std::string>(1);
        tenant.name      = stmt.column<std::string>(2);
        tenant.plan      = stmt.column<std::string>(3);
        tenant.active    = stmt.column<int>(4) != 0;
        return tenant;
    }

} // namespace repository
