#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_interceptor.h>
#include <memory>
#include <vector>

#include "interceptors/auth_interceptor.hpp"
#include "interceptors/logging_interceptor.hpp"
#include "interceptors/tenant_interceptor.hpp"

namespace multitenant {

// Factory for creating interceptor chains
class InterceptorFactory
    : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
  InterceptorFactory() = default;

  grpc::experimental::Interceptor *
  CreateServerInterceptor(grpc::experimental::ServerRpcInfo *info) override;
};

} // namespace multitenant
