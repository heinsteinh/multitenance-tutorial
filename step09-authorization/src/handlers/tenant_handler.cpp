#include "services/exceptions.hpp"
#include "services/mapper.hpp"
#include "services/tenant_service.hpp"

#include <grpcpp/grpcpp.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "tenant.grpc.pb.h"

namespace handlers {

class TenantHandler : public multitenant::v1::TenantService::Service {
public:
  explicit TenantHandler(services::TenantService &service)
      : service_(service) {}

  grpc::Status
  GetTenant(grpc::ServerContext * /*context*/,
            const multitenant::v1::GetTenantRequest *request,
            multitenant::v1::GetTenantResponse *response) override {
    try {
      auto tenant = service_.get_tenant(request->tenant_id());
      *response->mutable_tenant() = services::mapper::to_proto(tenant);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  ListTenants(grpc::ServerContext * /*context*/,
              const multitenant::v1::ListTenantsRequest * /*request*/,
              multitenant::v1::ListTenantsResponse *response) override {
    try {
      auto tenants = service_.list_tenants();
      for (const auto &tenant : tenants) {
        *response->add_tenants() = services::mapper::to_proto(tenant);
      }
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  CreateTenant(grpc::ServerContext * /*context*/,
               const multitenant::v1::CreateTenantRequest *request,
               multitenant::v1::CreateTenantResponse *response) override {
    try {
      auto dto = services::mapper::from_proto(*request);
      auto tenant = service_.create_tenant(dto);
      *response->mutable_tenant() = services::mapper::to_proto(tenant);
      spdlog::info("Created tenant '{}' ({})", tenant.name, tenant.tenant_id);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  UpdateTenant(grpc::ServerContext * /*context*/,
               const multitenant::v1::UpdateTenantRequest *request,
               multitenant::v1::UpdateTenantResponse *response) override {
    try {
      auto dto = services::mapper::from_proto(*request);
      auto tenant = service_.update_tenant(request->tenant_id(), dto);
      *response->mutable_tenant() = services::mapper::to_proto(tenant);
      spdlog::info("Updated tenant '{}'", tenant.tenant_id);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  DeleteTenant(grpc::ServerContext * /*context*/,
               const multitenant::v1::DeleteTenantRequest *request,
               multitenant::v1::DeleteTenantResponse * /*response*/) override {
    try {
      service_.delete_tenant(request->tenant_id());
      spdlog::info("Deleted tenant '{}'", request->tenant_id());
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

private:
  services::TenantService &service_;
};

} // namespace handlers

std::unique_ptr<multitenant::v1::TenantService::Service>
make_tenant_handler(services::TenantService &service) {
  return std::make_unique<handlers::TenantHandler>(service);
}
