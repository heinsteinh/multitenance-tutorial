#pragma once

#include "interceptors/base_interceptor.hpp"

#include <memory>
#include <optional>
#include <string>

namespace auth
{
    class JwtValidator;
}

namespace multitenant
{

    // Authentication interceptor that validates JWT bearer tokens
    class AuthInterceptor : public BaseServerInterceptor
    {
    public:
        explicit AuthInterceptor(std::shared_ptr<auth::JwtValidator> jwt_validator);

        void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

    private:
        std::shared_ptr<auth::JwtValidator> jwt_validator_;

        // Extract bearer token from authorization header
        std::optional<std::string>
        extract_bearer_token(const std::string& auth_header);
    };

} // namespace multitenant
