#pragma once

#include "auth/authorization_service.hpp"
#include "auth/jwt_validator.hpp"
#include "auth/role_repository.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace services
{

    /**
     * @brief Authentication and Authorization service
     * Coordinates JWT validation, role management, and permission checking
     */
    class AuthService
    {
    public:
        AuthService(std::shared_ptr<auth::JwtValidator> jwt_validator,
                    std::shared_ptr<auth::AuthorizationService> auth_service,
                    std::shared_ptr<auth::RoleRepository> role_repository);

        /**
         * @brief Validate JWT token and extract claims
         */
        std::optional<auth::TokenClaims> ValidateToken(const std::string& token);

        /**
         * @brief Generate JWT token for user with roles
         */
        std::string GenerateToken(int64_t user_id, const std::string& tenant_id,
                                  const std::vector<std::string>& roles,
                                  int expires_seconds = 3600);

        /**
         * @brief Check if user has permission
         */
        bool HasPermission(int64_t user_id, const std::string& resource,
                           const std::string& action);

        /**
         * @brief Check access with resource ownership
         */
        bool CanAccess(int64_t user_id, const std::string& resource,
                       const std::string& action, int64_t resource_owner_id);

        /**
         * @brief Create a new role
         */
        auth::Role CreateRole(const std::string& role_name,
                              const std::optional<std::string>& parent_role = {});

        /**
         * @brief Add permission to role
         */
        void AddPermissionToRole(const std::string& role_name,
                                 const std::string& resource,
                                 const std::string& action);

        /**
         * @brief Assign role to user
         */
        void AssignRoleToUser(int64_t user_id, const std::string& role_name);

        /**
         * @brief Get user roles
         */
        std::vector<auth::Role> GetUserRoles(int64_t user_id);

        /**
         * @brief Get effective permissions for user (including inherited)
         */
        std::vector<auth::Permission> GetEffectivePermissions(int64_t user_id);

    private:
        std::shared_ptr<auth::JwtValidator> jwt_validator_;
        std::shared_ptr<auth::AuthorizationService> auth_service_;
        std::shared_ptr<auth::RoleRepository> role_repository_;
    };

} // namespace services
