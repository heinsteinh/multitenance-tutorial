#pragma once

#include "dto.hpp"
#include "exceptions.hpp"
#include "repository/tenant_repository.hpp"

#include <memory>
#include <vector>

namespace services
{

    /**
     * Tenant service with repository-backed persistence
     */
    class TenantService
    {
    public:
        /**
         * Construct with repository for database-backed storage
         */
        explicit TenantService(std::shared_ptr<repository::TenantRepository> repository);

        /**
         * Default constructor for in-memory storage (testing)
         */
        TenantService();

        TenantModel get_tenant(const std::string& tenant_id) const;
        std::vector<TenantModel> list_tenants() const;
        TenantModel create_tenant(const CreateTenantDto& dto);
        TenantModel update_tenant(const std::string& tenant_id,
                                  const UpdateTenantDto& dto);
        void delete_tenant(const std::string& tenant_id);

    private:
        std::shared_ptr<repository::TenantRepository> repository_;

        // In-memory storage for testing mode
        bool use_memory_{false};
        mutable std::int64_t next_id_{1};
        mutable std::vector<TenantModel> tenants_;

        // Helper for in-memory mode
        TenantModel* find_tenant_by_id(const std::string& tenant_id) const;
    };

} // namespace services
