#pragma once

#include "interceptors/base_interceptor.hpp"
#include <string>

namespace multitenant {

// Interceptor for extracting and validating tenant context
class TenantInterceptor : public BaseServerInterceptor {
public:
  TenantInterceptor() = default;

  void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override;

private:
  // Validate tenant exists and is active (simplified for demo)
  bool is_tenant_active(const std::string &tenant_id);
};

} // namespace multitenant
