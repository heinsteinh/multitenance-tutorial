#pragma once

#include "authorization_service.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace db
{
    class Database;
}

namespace auth
{

    /**
     * @brief Repository for role management with database backing
     */
    class RoleRepository
    {
    public:
        explicit RoleRepository(std::shared_ptr<db::Database> database);

        /**
         * @brief Fetch role by name
         * @param role_name Name of the role
         * @return Role if found, empty optional otherwise
         */
        std::optional<Role> GetRole(const std::string& role_name);

        /**
         * @brief Create new role with optional parent role for inheritance
         * @param role_name Name of new role
         * @param parent_role Optional parent role for inheritance
         * @return Created role
         */
        Role CreateRole(const std::string& role_name,
                        const std::optional<std::string>& parent_role = {});

        /**
         * @brief Add permission to role
         * @param role_name Name of the role
         * @param resource Resource name (e.g., "users", "orders")
         * @param action Action name (e.g., "create", "read", "update", "delete")
         */
        void AddPermission(const std::string& role_name, const std::string& resource,
                           const std::string& action);

        /**
         * @brief Remove permission from role
         */
        void RemovePermission(const std::string& role_name,
                              const std::string& resource, const std::string& action);

        /**
         * @brief Get all permissions for role (including inherited)
         */
        std::vector<Permission> GetRolePermissions(const std::string& role_name);

        /**
         * @brief Get all roles for user
         */
        std::vector<Role> GetUserRoles(int64_t user_id);

        /**
         * @brief Assign role to user
         */
        void AssignRoleToUser(int64_t user_id, const std::string& role_name);

        /**
         * @brief Remove role from user
         */
        void RemoveRoleFromUser(int64_t user_id, const std::string& role_name);

        /**
         * @brief Get all users with specific role
         */
        std::vector<int64_t> GetUsersWithRole(const std::string& role_name);

    private:
        std::shared_ptr<db::Database> database_;

        /**
         * @brief Recursively collect permissions from role and parent roles
         */
        std::vector<Permission> CollectPermissionsRecursive(
            const std::string& role_name);
    };

} // namespace auth
