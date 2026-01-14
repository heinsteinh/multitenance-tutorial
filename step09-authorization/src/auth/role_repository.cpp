#include "auth/role_repository.hpp"

#include <db/database.hpp>
#include <spdlog/spdlog.h>

namespace auth
{

    RoleRepository::RoleRepository(std::shared_ptr<db::Database> database)
        : database_(std::move(database))
    {
        // Initialize database schema if needed
        try
        {
            if(!database_->table_exists("roles"))
            {
                database_->execute(R"(
        CREATE TABLE roles (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          tenant_id TEXT NOT NULL,
          name TEXT NOT NULL,
          parent_role TEXT,
          created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
          UNIQUE(tenant_id, name)
        )
      )");
                spdlog::info("Created roles table");
            }

            if(!database_->table_exists("role_permissions"))
            {
                database_->execute(R"(
        CREATE TABLE role_permissions (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          role_id INTEGER NOT NULL,
          resource TEXT NOT NULL,
          action TEXT NOT NULL,
          created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
          FOREIGN KEY(role_id) REFERENCES roles(id),
          UNIQUE(role_id, resource, action)
        )
      )");
                spdlog::info("Created role_permissions table");
            }

            if(!database_->table_exists("user_roles"))
            {
                database_->execute(R"(
        CREATE TABLE user_roles (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          user_id INTEGER NOT NULL,
          role_id INTEGER NOT NULL,
          assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
          UNIQUE(user_id, role_id)
        )
      )");
                spdlog::info("Created user_roles table");
            }
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error initializing role schema: {}", e.what());
            throw;
        }
    }

    std::optional<Role> RoleRepository::GetRole(const std::string& role_name)
    {
        try
        {
            auto stmt = database_->prepare(
                "SELECT id, name, parent_role FROM roles WHERE name = ? LIMIT 1");
            stmt.bind(1, role_name);

            if(!stmt.step())
            {
                return {};
            }

            Role role;
            role.id   = stmt.column<int64_t>(0);
            role.name = stmt.column<std::string>(1);

            auto parent = stmt.column<std::string>(2);
            if(!parent.empty())
            {
                role.parent_role = parent;
            }

            // Load permissions
            role.permissions = GetRolePermissions(role_name);

            return role;
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error getting role: {}", e.what());
            return {};
        }
    }

    Role RoleRepository::CreateRole(const std::string& role_name,
                                    const std::optional<std::string>& parent_role)
    {
        try
        {
            auto stmt = database_->prepare(
                "INSERT INTO roles (tenant_id, name, parent_role) VALUES (?, ?, ?)");
            stmt.bind(1, "default"); // Default tenant for now
            stmt.bind(2, role_name);
            if(parent_role)
            {
                stmt.bind(3, parent_role.value());
            }
            stmt.step();

            Role role;
            role.id          = database_->last_insert_rowid();
            role.name        = role_name;
            role.parent_role = parent_role;
            role.tenant_id   = "default";

            spdlog::info("Created role {} with id {}", role_name, role.id);
            return role;
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error creating role: {}", e.what());
            throw;
        }
    }

    void RoleRepository::AddPermission(const std::string& role_name,
                                       const std::string& resource,
                                       const std::string& action)
    {
        try
        {
            auto role = GetRole(role_name);
            if(!role)
            {
                throw std::runtime_error("Role not found: " + role_name);
            }

            auto stmt = database_->prepare(
                "INSERT INTO role_permissions (role_id, resource, action) VALUES (?, "
                "?, ?)");
            stmt.bind(1, role->id);
            stmt.bind(2, resource);
            stmt.bind(3, action);
            stmt.step();

            spdlog::debug("Added permission {}/{} to role {}", resource, action,
                          role_name);
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error adding permission: {}", e.what());
            throw;
        }
    }

    void RoleRepository::RemovePermission(const std::string& role_name,
                                          const std::string& resource,
                                          const std::string& action)
    {
        try
        {
            auto role = GetRole(role_name);
            if(!role)
            {
                throw std::runtime_error("Role not found: " + role_name);
            }

            auto stmt = database_->prepare(
                "DELETE FROM role_permissions WHERE role_id = ? AND resource = ? AND "
                "action = ?");
            stmt.bind(1, role->id);
            stmt.bind(2, resource);
            stmt.bind(3, action);
            stmt.step();

            spdlog::debug("Removed permission {}/{} from role {}", resource, action,
                          role_name);
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error removing permission: {}", e.what());
            throw;
        }
    }

    std::vector<Permission> RoleRepository::GetRolePermissions(
        const std::string& role_name)
    {
        return CollectPermissionsRecursive(role_name);
    }

    std::vector<Permission> RoleRepository::CollectPermissionsRecursive(
        const std::string& role_name)
    {
        std::vector<Permission> permissions;

        try
        {
            // Get this role's permissions
            auto stmt = database_->prepare(R"(
      SELECT rp.resource, rp.action
      FROM role_permissions rp
      JOIN roles r ON rp.role_id = r.id
      WHERE r.name = ?
    )");
            stmt.bind(1, role_name);

            while(stmt.step())
            {
                Permission perm;
                perm.resource = stmt.column<std::string>(0);
                perm.action   = stmt.column<std::string>(1);
                permissions.push_back(perm);
            }

            // Get parent role's permissions (inheritance)
            auto parent_stmt = database_->prepare(
                "SELECT parent_role FROM roles WHERE name = ? LIMIT 1");
            parent_stmt.bind(1, role_name);

            if(parent_stmt.step())
            {
                auto parent = parent_stmt.column<std::string>(0);
                if(!parent.empty())
                {
                    auto parent_perms = CollectPermissionsRecursive(parent);
                    for(const auto& perm : parent_perms)
                    {
                        // Avoid duplicates
                        if(std::find(permissions.begin(), permissions.end(), perm) == permissions.end())
                        {
                            permissions.push_back(perm);
                        }
                    }
                }
            }
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error collecting permissions: {}", e.what());
        }

        return permissions;
    }

    std::vector<Role> RoleRepository::GetUserRoles(int64_t user_id)
    {
        std::vector<Role> roles;

        try
        {
            auto stmt = database_->prepare(R"(
      SELECT r.id, r.name, r.parent_role
      FROM roles r
      JOIN user_roles ur ON r.id = ur.role_id
      WHERE ur.user_id = ?
    )");
            stmt.bind(1, user_id);

            while(stmt.step())
            {
                Role role;
                role.id   = stmt.column<int64_t>(0);
                role.name = stmt.column<std::string>(1);

                auto parent = stmt.column<std::string>(2);
                if(!parent.empty())
                {
                    role.parent_role = parent;
                }

                role.permissions = GetRolePermissions(role.name);
                roles.push_back(role);
            }

            spdlog::debug("User {} has {} roles", user_id, roles.size());
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error getting user roles: {}", e.what());
        }

        return roles;
    }

    void RoleRepository::AssignRoleToUser(int64_t user_id,
                                          const std::string& role_name)
    {
        try
        {
            auto role = GetRole(role_name);
            if(!role)
            {
                throw std::runtime_error("Role not found: " + role_name);
            }

            auto stmt = database_->prepare(
                "INSERT INTO user_roles (user_id, role_id) VALUES (?, ?)");
            stmt.bind(1, user_id);
            stmt.bind(2, role->id);
            stmt.step();

            spdlog::info("Assigned role {} to user {}", role_name, user_id);
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error assigning role: {}", e.what());
            throw;
        }
    }

    void RoleRepository::RemoveRoleFromUser(int64_t user_id,
                                            const std::string& role_name)
    {
        try
        {
            auto role = GetRole(role_name);
            if(!role)
            {
                throw std::runtime_error("Role not found: " + role_name);
            }

            auto stmt = database_->prepare(
                "DELETE FROM user_roles WHERE user_id = ? AND role_id = ?");
            stmt.bind(1, user_id);
            stmt.bind(2, role->id);
            stmt.step();

            spdlog::info("Removed role {} from user {}", role_name, user_id);
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error removing role: {}", e.what());
            throw;
        }
    }

    std::vector<int64_t> RoleRepository::GetUsersWithRole(
        const std::string& role_name)
    {
        std::vector<int64_t> users;

        try
        {
            auto stmt = database_->prepare(R"(
      SELECT ur.user_id
      FROM user_roles ur
      JOIN roles r ON ur.role_id = r.id
      WHERE r.name = ?
    )");
            stmt.bind(1, role_name);

            while(stmt.step())
            {
                users.push_back(stmt.column<int64_t>(0));
            }

            spdlog::debug("Found {} users with role {}", users.size(), role_name);
        }
        catch(const std::exception& e)
        {
            spdlog::error("Error getting users with role: {}", e.what());
        }

        return users;
    }

} // namespace auth
