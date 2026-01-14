#include "interceptors/tenant_interceptor.hpp"
#include <spdlog/spdlog.h>

namespace multitenant {

void TenantInterceptor::Intercept(
    grpc::experimental::InterceptorBatchMethods *methods) {
  if (methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::
              PRE_RECV_INITIAL_METADATA)) {

    auto *metadata = methods->GetRecvInitialMetadata();
    auto tenant_id = get_metadata(metadata, "x-tenant-id");

    if (tenant_id.has_value()) {
      // Validate tenant is active
      if (!is_tenant_active(tenant_id.value())) {
        spdlog::warn("Tenant validation failed: {} is not active",
                     tenant_id.value());
        // Don't proceed - skip this request
        return;
      }

      spdlog::debug("Tenant context set: {}", tenant_id.value());
      // In real app, would set tenant context in thread-local storage
    } else {
      spdlog::debug("No tenant ID provided in request headers");
    }
  }

  methods->Proceed();
}

bool TenantInterceptor::is_tenant_active(const std::string &tenant_id) {
  // Simplified validation - in real app would check database
  // For demo, accept "demo" and any tenant with "test" prefix
  return tenant_id == "demo" || tenant_id.find("test") == 0 ||
         tenant_id.find("tenant") == 0;
}

} // namespace multitenant
