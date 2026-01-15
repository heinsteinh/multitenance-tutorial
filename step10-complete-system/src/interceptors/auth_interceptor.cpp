#include "interceptors/auth_interceptor.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace multitenant
{

    AuthInterceptor::AuthInterceptor(std::shared_ptr<auth::JwtValidator> jwt_validator)
        : jwt_validator_(std::move(jwt_validator))
    {
    }

    void AuthInterceptor::Intercept(
        grpc::experimental::InterceptorBatchMethods* methods)
    {
        if(methods->QueryInterceptionHookPoint(
               grpc::experimental::InterceptionHookPoints::PRE_RECV_INITIAL_METADATA))
        {

            auto* metadata = methods->GetRecvInitialMetadata();

            // Extract and validate authorization header
            auto auth_header = get_metadata(metadata, "authorization");
            if(!auth_header.has_value())
            {
                spdlog::debug("Missing authorization header");
                // Continue anyway - some endpoints might be public
                methods->Proceed();
                return;
            }

            // Extract bearer token
            auto token = extract_bearer_token(auth_header.value());
            if(!token.has_value())
            {
                spdlog::warn("Invalid authorization header format");
                methods->Proceed();
                return;
            }

            // Validate JWT token
            if(jwt_validator_)
            {
                auto claims = jwt_validator_->Validate(token.value());
                if(!claims.has_value())
                {
                    spdlog::warn("JWT validation failed for request");
                    // In production, we would reject the request here
                    // For now, just log and proceed for demo
                }
                else
                {
                    spdlog::debug("JWT validated for user {} in tenant {}",
                                  claims->user_id, claims->tenant_id);
                }
            }
            else
            {
                spdlog::debug("No JWT validator configured, skipping validation");
            }
        }

        methods->Proceed();
    }

    std::optional<std::string>
    AuthInterceptor::extract_bearer_token(const std::string& auth_header)
    {
        // Expected format: "Bearer <token>"
        const std::string prefix = "Bearer ";

        if(auth_header.size() <= prefix.size())
        {
            return std::nullopt;
        }

        if(auth_header.substr(0, prefix.size()) != prefix)
        {
            return std::nullopt;
        }

        return auth_header.substr(prefix.size());
    }

} // namespace multitenant
