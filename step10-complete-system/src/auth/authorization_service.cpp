#include "auth/authorization_service.hpp"

#include "auth/role_repository.hpp"

#include <spdlog/spdlog.h>

namespace auth
{

    class AuthorizationServiceImpl : public AuthorizationService
    {
    public:
        explicit AuthorizationServiceImpl(
            std::shared_ptr<RoleRepository> role_repository)
            : role_repository_(std::move(role_repository))
        {
        }

        bool HasPermission(int64_t user_id, const std::string& resource,
                           const std::string& action) override
        {
            try
            {
                auto roles = role_repository_->GetUserRoles(user_id);

                for(const auto& role : roles)
                {
                    auto permissions = role_repository_->GetRolePermissions(role.name);
                    for(const auto& perm : permissions)
                    {
                        if(perm.resource == resource && perm.action == action)
                        {
                            spdlog::debug(
                                "User {} has permission for {}/{}", user_id, resource, action);
                            return true;
                        }
                    }
                }

                spdlog::debug("User {} denied permission for {}/{}", user_id, resource,
                              action);
                return false;
            }
            catch(const std::exception& e)
            {
                spdlog::error("Error checking permission: {}", e.what());
                return false;
            }
        }

        bool CanAccess(int64_t user_id, const std::string& resource,
                       const std::string& action, int64_t resource_owner_id) override
        {
            // Owner can always modify their own resources
            if(user_id == resource_owner_id && (action == "update" || action == "delete"))
            {
                spdlog::debug("User {} (owner) can {} resource", user_id, action);
                return true;
            }

            // Fall back to permission check
            return HasPermission(user_id, resource, action);
        }

        void GrantRole(int64_t user_id, const std::string& role) override
        {
            try
            {
                role_repository_->AssignRoleToUser(user_id, role);
                spdlog::info("Granted role {} to user {}", role, user_id);
            }
            catch(const std::exception& e)
            {
                spdlog::error("Error granting role: {}", e.what());
                throw;
            }
        }

        void RevokeRole(int64_t user_id, const std::string& role) override
        {
            try
            {
                role_repository_->RemoveRoleFromUser(user_id, role);
                spdlog::info("Revoked role {} from user {}", role, user_id);
            }
            catch(const std::exception& e)
            {
                spdlog::error("Error revoking role: {}", e.what());
                throw;
            }
        }

        std::vector<Permission> GetEffectivePermissions(
            int64_t user_id) override
        {
            try
            {
                auto roles = role_repository_->GetUserRoles(user_id);
                std::vector<Permission> effective_perms;

                for(const auto& role : roles)
                {
                    auto perms = role_repository_->GetRolePermissions(role.name);
                    for(const auto& perm : perms)
                    {
                        // Avoid duplicates
                        if(std::find(effective_perms.begin(), effective_perms.end(), perm) == effective_perms.end())
                        {
                            effective_perms.push_back(perm);
                        }
                    }
                }

                spdlog::debug("User {} has {} effective permissions", user_id,
                              effective_perms.size());
                return effective_perms;
            }
            catch(const std::exception& e)
            {
                spdlog::error("Error getting effective permissions: {}", e.what());
                return {};
            }
        }

        std::vector<Role> GetUserRoles(int64_t user_id) override
        {
            try
            {
                return role_repository_->GetUserRoles(user_id);
            }
            catch(const std::exception& e)
            {
                spdlog::error("Error getting user roles: {}", e.what());
                return {};
            }
        }

    private:
        std::shared_ptr<RoleRepository> role_repository_;
    };

    // Factory function
    std::shared_ptr<AuthorizationService> CreateAuthorizationService(
        std::shared_ptr<RoleRepository> role_repository)
    {
        return std::make_shared<AuthorizationServiceImpl>(role_repository);
    }

} // namespace auth
