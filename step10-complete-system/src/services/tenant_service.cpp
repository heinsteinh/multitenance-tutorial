#include "services/tenant_service.hpp"

#include <algorithm>

namespace services
{

    TenantService::TenantService(std::shared_ptr<repository::TenantRepository> repository)
        : repository_(std::move(repository))
        , use_memory_(false)
    {
    }

    TenantService::TenantService()
        : repository_(nullptr)
        , use_memory_(true)
    {
        // seed with a default tenant for demo purposes
        TenantModel demo{
            .id        = next_id_++,
            .tenant_id = "demo",
            .name      = "Demo Tenant",
            .plan      = "basic",
            .active    = true,
        };
        tenants_.push_back(demo);
    }

    TenantModel* TenantService::find_tenant_by_id(const std::string& tenant_id) const
    {
        auto it = std::find_if(tenants_.begin(), tenants_.end(),
                               [&](const TenantModel& t) { return t.tenant_id == tenant_id; });
        if(it == tenants_.end())
        {
            return nullptr;
        }
        return &(*it);
    }

    TenantModel TenantService::get_tenant(const std::string& tenant_id) const
    {
        if(use_memory_)
        {
            auto* tenant = find_tenant_by_id(tenant_id);
            if(!tenant)
            {
                throw NotFoundException("Tenant not found");
            }
            return *tenant;
        }

        auto tenant = repository_->find_by_tenant_id(tenant_id);
        if(!tenant)
        {
            throw NotFoundException("Tenant not found");
        }
        return tenant.value();
    }

    std::vector<TenantModel> TenantService::list_tenants() const
    {
        if(use_memory_)
        {
            return tenants_;
        }

        return repository_->find_all();
    }

    TenantModel TenantService::create_tenant(const CreateTenantDto& dto)
    {
        if(dto.tenant_id.empty())
        {
            throw ValidationException("tenant_id is required");
        }
        if(dto.name.empty())
        {
            throw ValidationException("Tenant name is required");
        }

        if(use_memory_)
        {
            auto exists = std::any_of(
                tenants_.begin(), tenants_.end(),
                [&](const TenantModel& t) { return t.tenant_id == dto.tenant_id; });
            if(exists)
            {
                throw ValidationException("Tenant already exists");
            }

            TenantModel tenant{
                .id        = next_id_++,
                .tenant_id = dto.tenant_id,
                .name      = dto.name,
                .plan      = dto.plan.empty() ? "free" : dto.plan,
                .active    = dto.active,
            };

            tenants_.push_back(tenant);
            return tenant;
        }

        // Check tenant_id uniqueness
        if(repository_->tenant_id_exists(dto.tenant_id))
        {
            throw ValidationException("Tenant already exists");
        }

        TenantModel tenant{
            .id        = 0,
            .tenant_id = dto.tenant_id,
            .name      = dto.name,
            .plan      = dto.plan.empty() ? "free" : dto.plan,
            .active    = dto.active,
        };

        auto id   = repository_->insert(tenant);
        tenant.id = id;
        return tenant;
    }

    TenantModel TenantService::update_tenant(const std::string& tenant_id,
                                             const UpdateTenantDto& dto)
    {
        if(use_memory_)
        {
            auto* tenant = find_tenant_by_id(tenant_id);
            if(!tenant)
            {
                throw NotFoundException("Tenant not found");
            }

            if(dto.name.has_value())
            {
                tenant->name = dto.name.value();
            }
            if(dto.plan.has_value())
            {
                tenant->plan = dto.plan.value();
            }
            if(dto.active.has_value())
            {
                tenant->active = dto.active.value();
            }

            return *tenant;
        }

        auto existing = repository_->find_by_tenant_id(tenant_id);
        if(!existing)
        {
            throw NotFoundException("Tenant not found");
        }

        auto tenant = existing.value();

        if(dto.name.has_value())
        {
            tenant.name = dto.name.value();
        }
        if(dto.plan.has_value())
        {
            tenant.plan = dto.plan.value();
        }
        if(dto.active.has_value())
        {
            tenant.active = dto.active.value();
        }

        repository_->update(tenant);
        return tenant;
    }

    void TenantService::delete_tenant(const std::string& tenant_id)
    {
        if(use_memory_)
        {
            auto* tenant = find_tenant_by_id(tenant_id);
            if(!tenant)
            {
                throw NotFoundException("Tenant not found");
            }
            tenant->active = false;
            return;
        }

        auto existing = repository_->find_by_tenant_id(tenant_id);
        if(!existing)
        {
            throw NotFoundException("Tenant not found");
        }

        // Soft delete by deactivating
        repository_->deactivate(tenant_id);
    }

} // namespace services
