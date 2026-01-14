#pragma once

#include "services/auth_service.hpp"

#include <grpc++/server_context.h>
#include <memory>
#include <optional>

namespace handlers
{

    /**
     * @brief Helper for authorization operations in service handlers
     * Can be used by user_handler and tenant_handler to check permissions
     */
    class AuthorizationHelper
    {
    public:
        explicit AuthorizationHelper(std::shared_ptr<services::AuthService> auth_service);

        /**
         * @brief Extract and validate JWT token from gRPC metadata
         */
        std::optional<auth::TokenClaims> ValidateRequestToken(
            grpc::ServerContext* context);

        /**
         * @brief Check if authenticated user has permission
         */
        bool CheckPermission(int64_t user_id, const std::string& resource,
                             const std::string& action);

    private:
        std::shared_ptr<services::AuthService> auth_service_;
    };

} // namespace handlers
