#include "handlers/auth_handler.hpp"

#include <spdlog/spdlog.h>

namespace handlers
{

    AuthorizationHelper::AuthorizationHelper(
        std::shared_ptr<services::AuthService> auth_service)
        : auth_service_(std::move(auth_service))
    {
    }

    std::optional<auth::TokenClaims> AuthorizationHelper::ValidateRequestToken(
        grpc::ServerContext* context)
    {
        if(!context)
        {
            spdlog::warn("No server context provided");
            return {};
        }

        // Get authorization header from metadata
        auto metadata  = context->client_metadata();
        auto auth_iter = metadata.find("authorization");

        if(auth_iter == metadata.end())
        {
            spdlog::debug("No authorization header in request");
            return {};
        }

        std::string auth_header(auth_iter->second.data(),
                                auth_iter->second.length());

        // Extract bearer token
        const std::string prefix = "Bearer ";
        if(auth_header.size() <= prefix.size() || auth_header.substr(0, prefix.size()) != prefix)
        {
            spdlog::warn("Invalid authorization header format");
            return {};
        }

        std::string token = auth_header.substr(prefix.size());

        // Validate JWT
        return auth_service_->ValidateToken(token);
    }

    bool AuthorizationHelper::CheckPermission(int64_t user_id,
                                              const std::string& resource,
                                              const std::string& action)
    {
        bool has_permission = auth_service_->HasPermission(user_id, resource, action);

        if(!has_permission)
        {
            spdlog::warn("User {} denied permission for {}/{}", user_id, resource,
                         action);
        }

        return has_permission;
    }

} // namespace handlers
