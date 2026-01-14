#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace auth
{

    /**
     * @brief Represents a permission in the system
     */
    struct Permission
    {
        std::string resource; // e.g., "users", "orders", "reports"
        std::string action;   // e.g., "create", "read", "update", "delete"

        bool operator==(const Permission& other) const
        {
            return resource == other.resource && action == other.action;
        }
    };

    /**
     * @brief Represents a role with permissions
     */
    struct Role
    {
        int64_t id = 0;
        std::string tenant_id;
        std::string name;
        std::vector<Permission> permissions;
        std::optional<std::string> parent_role;

        bool operator==(const Role& other) const
        {
            return id == other.id && name == other.name;
        }
    };

    /**
     * @brief Authorization context for requests
     */
    struct AuthContext
    {
        int64_t user_id = 0;
        std::string tenant_id;
        std::vector<std::string> roles;
        int64_t expires_at = 0;
    };

    /**
     * @brief Resource access request
     */
    struct Resource
    {
        std::string resource_type; // e.g., "users", "orders"
        int64_t resource_id = 0;
        int64_t owner_id    = 0;
        std::string tenant_id;
    };

    /**
     * @brief Authorization service for RBAC operations
     */
    class AuthorizationService
    {
    public:
        virtual ~AuthorizationService() = default;

        /**
         * @brief Check if user has permission for resource/action
         */
        virtual bool HasPermission(int64_t user_id, const std::string& resource,
                                   const std::string& action) = 0;

        /**
         * @brief Check access with resource ownership
         */
        virtual bool CanAccess(int64_t user_id, const std::string& resource,
                               const std::string& action,
                               int64_t resource_owner_id) = 0;

        /**
         * @brief Grant role to user
         */
        virtual void GrantRole(int64_t user_id, const std::string& role) = 0;

        /**
         * @brief Revoke role from user
         */
        virtual void RevokeRole(int64_t user_id, const std::string& role) = 0;

        /**
         * @brief Get effective permissions (including inherited)
         */
        virtual std::vector<Permission> GetEffectivePermissions(
            int64_t user_id) = 0;

        /**
         * @brief Get user's roles
         */
        virtual std::vector<Role> GetUserRoles(int64_t user_id) = 0;
    };

    // Forward declarations
    class RoleRepository;

    /**
     * @brief Factory function to create authorization service
     */
    std::shared_ptr<AuthorizationService> CreateAuthorizationService(
        std::shared_ptr<RoleRepository> role_repository);

} // namespace auth
