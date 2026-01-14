#include "services/auth_service.hpp"

#include <spdlog/spdlog.h>

namespace services
{

    AuthService::AuthService(std::shared_ptr<auth::JwtValidator> jwt_validator,
                             std::shared_ptr<auth::AuthorizationService> auth_service,
                             std::shared_ptr<auth::RoleRepository> role_repository)
        : jwt_validator_(std::move(jwt_validator))
        , auth_service_(std::move(auth_service))
        , role_repository_(std::move(role_repository))
    {
    }

    std::optional<auth::TokenClaims> AuthService::ValidateToken(
        const std::string& token)
    {
        return jwt_validator_->Validate(token);
    }

    std::string AuthService::GenerateToken(int64_t user_id,
                                           const std::string& tenant_id,
                                           const std::vector<std::string>& roles,
                                           int expires_seconds)
    {
        int64_t now = jwt_validator_->GetCurrentTime();
        auth::TokenClaims claims{
            .user_id    = user_id,
            .tenant_id  = tenant_id,
            .roles      = roles,
            .issued_at  = now,
            .expires_at = now + expires_seconds,
        };

        std::string token = jwt_validator_->Generate(claims);
        spdlog::info("Generated token for user {} in tenant {}", user_id, tenant_id);
        return token;
    }

    bool AuthService::HasPermission(int64_t user_id, const std::string& resource,
                                    const std::string& action)
    {
        return auth_service_->HasPermission(user_id, resource, action);
    }

    bool AuthService::CanAccess(int64_t user_id, const std::string& resource,
                                const std::string& action,
                                int64_t resource_owner_id)
    {
        return auth_service_->CanAccess(user_id, resource, action, resource_owner_id);
    }

    auth::Role AuthService::CreateRole(const std::string& role_name,
                                       const std::optional<std::string>& parent_role)
    {
        return role_repository_->CreateRole(role_name, parent_role);
    }

    void AuthService::AddPermissionToRole(const std::string& role_name,
                                          const std::string& resource,
                                          const std::string& action)
    {
        role_repository_->AddPermission(role_name, resource, action);
    }

    void AuthService::AssignRoleToUser(int64_t user_id,
                                       const std::string& role_name)
    {
        auth_service_->GrantRole(user_id, role_name);
    }

    std::vector<auth::Role> AuthService::GetUserRoles(int64_t user_id)
    {
        return auth_service_->GetUserRoles(user_id);
    }

    std::vector<auth::Permission> AuthService::GetEffectivePermissions(
        int64_t user_id)
    {
        return auth_service_->GetEffectivePermissions(user_id);
    }

} // namespace services
