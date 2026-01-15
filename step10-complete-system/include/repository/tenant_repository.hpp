#pragma once

#include "db/database.hpp"
#include "services/dto.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace repository
{

    /**
     * Repository for tenant data persistence
     */
    class TenantRepository
    {
    public:
        explicit TenantRepository(std::shared_ptr<db::Database> database);

        /**
         * Initialize the tenants table schema
         */
        void initialize_schema();

        /**
         * Find tenant by ID
         */
        std::optional<services::TenantModel> find_by_id(int64_t id);

        /**
         * Find tenant by tenant_id (string identifier)
         */
        std::optional<services::TenantModel> find_by_tenant_id(
            const std::string& tenant_id);

        /**
         * Find all tenants
         */
        std::vector<services::TenantModel> find_all();

        /**
         * Find all active tenants
         */
        std::vector<services::TenantModel> find_active();

        /**
         * Insert a new tenant
         * @return The ID of the inserted tenant
         */
        int64_t insert(const services::TenantModel& tenant);

        /**
         * Update an existing tenant
         */
        void update(const services::TenantModel& tenant);

        /**
         * Delete a tenant by ID
         */
        void remove(int64_t id);

        /**
         * Check if tenant_id exists
         */
        bool tenant_id_exists(const std::string& tenant_id);

        /**
         * Check if tenant is active
         */
        bool is_active(const std::string& tenant_id);

        /**
         * Activate a tenant
         */
        void activate(const std::string& tenant_id);

        /**
         * Deactivate a tenant
         */
        void deactivate(const std::string& tenant_id);

    private:
        std::shared_ptr<db::Database> database_;

        services::TenantModel map_from_row(db::Statement& stmt);
    };

} // namespace repository
