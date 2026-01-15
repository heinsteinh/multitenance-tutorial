#pragma once

#include "auth/jwt_validator.hpp"
#include "interceptors/auth_interceptor.hpp"
#include "interceptors/logging_interceptor.hpp"
#include "interceptors/tenant_interceptor.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_interceptor.h>
#include <memory>
#include <vector>

namespace multitenant
{

    /**
     * Factory for creating interceptor chains
     */
    class InterceptorFactory
        : public grpc::experimental::ServerInterceptorFactoryInterface
    {
    public:
        InterceptorFactory() = default;
        explicit InterceptorFactory(std::shared_ptr<auth::JwtValidator> jwt_validator);

        grpc::experimental::Interceptor*
        CreateServerInterceptor(grpc::experimental::ServerRpcInfo* info) override;

    private:
        std::shared_ptr<auth::JwtValidator> jwt_validator_;
    };

} // namespace multitenant
