#include "services/tenant_service.hpp"

#include <algorithm>

namespace services {

TenantService::TenantService() : next_id_(1) {
  // seed with a default tenant for demo purposes
  TenantModel demo{
      .id = next_id_++,
      .tenant_id = "demo",
      .name = "Demo Tenant",
      .plan = "basic",
      .active = true,
  };
  tenants_.push_back(demo);
}

TenantModel TenantService::get_tenant(const std::string &tenant_id) const {
  auto it =
      std::find_if(tenants_.begin(), tenants_.end(), [&](const TenantModel &t) {
        return t.tenant_id == tenant_id;
      });
  if (it == tenants_.end()) {
    throw NotFoundException("Tenant not found");
  }
  return *it;
}

std::vector<TenantModel> TenantService::list_tenants() const {
  return tenants_;
}

TenantModel TenantService::create_tenant(const CreateTenantDto &dto) {
  if (dto.tenant_id.empty()) {
    throw ValidationException("tenant_id is required");
  }
  if (dto.name.empty()) {
    throw ValidationException("Tenant name is required");
  }

  auto exists =
      std::any_of(tenants_.begin(), tenants_.end(), [&](const TenantModel &t) {
        return t.tenant_id == dto.tenant_id;
      });
  if (exists) {
    throw ValidationException("Tenant already exists");
  }

  TenantModel tenant{
      .id = next_id_++,
      .tenant_id = dto.tenant_id,
      .name = dto.name,
      .plan = dto.plan.empty() ? "free" : dto.plan,
      .active = dto.active,
  };

  tenants_.push_back(tenant);
  return tenant;
}

TenantModel TenantService::update_tenant(const std::string &tenant_id,
                                         const UpdateTenantDto &dto) {
  auto it =
      std::find_if(tenants_.begin(), tenants_.end(), [&](const TenantModel &t) {
        return t.tenant_id == tenant_id;
      });
  if (it == tenants_.end()) {
    throw NotFoundException("Tenant not found");
  }

  // Update fields if provided
  if (dto.name.has_value()) {
    it->name = dto.name.value();
  }
  if (dto.plan.has_value()) {
    it->plan = dto.plan.value();
  }
  if (dto.active.has_value()) {
    it->active = dto.active.value();
  }

  return *it;
}

void TenantService::delete_tenant(const std::string &tenant_id) {
  auto it =
      std::find_if(tenants_.begin(), tenants_.end(), [&](const TenantModel &t) {
        return t.tenant_id == tenant_id;
      });
  if (it == tenants_.end()) {
    throw NotFoundException("Tenant not found");
  }

  // Soft delete - just deactivate the tenant
  it->active = false;
}

} // namespace services
