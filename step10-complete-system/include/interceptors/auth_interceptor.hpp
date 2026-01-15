#pragma once

#include "auth/jwt_validator.hpp"
#include "interceptors/base_interceptor.hpp"

#include <memory>
#include <optional>
#include <string>

namespace multitenant
{

    /**
     * Authentication interceptor that validates JWT bearer tokens
     */
    class AuthInterceptor : public BaseServerInterceptor
    {
    public:
        AuthInterceptor() = default;
        explicit AuthInterceptor(std::shared_ptr<auth::JwtValidator> jwt_validator);

        void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

    private:
        std::shared_ptr<auth::JwtValidator> jwt_validator_;

        // Extract bearer token from authorization header
        std::optional<std::string> extract_bearer_token(const std::string& auth_header);
    };

} // namespace multitenant
