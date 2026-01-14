#pragma once

#include "dto.hpp"
#include "exceptions.hpp"

#include <vector>

namespace services {

class TenantService {
public:
  TenantService();

  TenantModel get_tenant(const std::string &tenant_id) const;
  std::vector<TenantModel> list_tenants() const;
  TenantModel create_tenant(const CreateTenantDto &dto);
  TenantModel update_tenant(const std::string &tenant_id,
                            const UpdateTenantDto &dto);
  void delete_tenant(const std::string &tenant_id);

private:
  std::int64_t next_id_;
  std::vector<TenantModel> tenants_;
};

} // namespace services
