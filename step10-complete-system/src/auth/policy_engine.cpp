#include "auth/policy_engine.hpp"

#include "auth/authorization_service.hpp"
#include "auth/jwt_validator.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace auth
{

    // OwnershipPolicy Implementation
    bool OwnershipPolicy::Evaluate(const TokenClaims& claims,
                                   const Resource& resource,
                                   const std::string& action)
    {
        // Ownership policy only applies to modify operations
        if(action != "update" && action != "delete")
        {
            return true; // Read operations don't require ownership
        }

        bool is_owner = claims.user_id == resource.owner_id;
        spdlog::debug("OwnershipPolicy: user {} owner {}: {}", claims.user_id,
                      resource.owner_id, is_owner);
        return is_owner;
    }

    // TenantIsolationPolicy Implementation
    bool TenantIsolationPolicy::Evaluate(const TokenClaims& claims,
                                         const Resource& resource,
                                         const std::string& action)
    {
        bool same_tenant = claims.tenant_id == resource.tenant_id;
        spdlog::debug("TenantIsolationPolicy: claim tenant {} resource tenant {}: {}",
                      claims.tenant_id, resource.tenant_id, same_tenant);
        return same_tenant;
    }

    // RolePolicy Implementation
    RolePolicy::RolePolicy(std::vector<std::string> required_roles)
        : required_roles_(std::move(required_roles))
    {
    }

    bool RolePolicy::Evaluate(const TokenClaims& claims, const Resource& resource,
                              const std::string& action)
    {
        // Check if user has any of the required roles
        for(const auto& required_role : required_roles_)
        {
            for(const auto& user_role : claims.roles)
            {
                if(user_role == required_role)
                {
                    spdlog::debug("RolePolicy: user has required role {}", required_role);
                    return true;
                }
            }
        }

        spdlog::debug("RolePolicy: user missing required roles");
        return false;
    }

    // PolicyEngine Implementation
    void PolicyEngine::AddPolicy(std::shared_ptr<Policy> policy)
    {
        policies_.push_back(policy);
        spdlog::debug("Added policy to engine, total: {}", policies_.size());
    }

    void PolicyEngine::ClearPolicies()
    {
        policies_.clear();
        spdlog::debug("Cleared all policies");
    }

    bool PolicyEngine::Evaluate(const TokenClaims& claims,
                                const Resource& resource,
                                const std::string& action)
    {
        if(policies_.empty())
        {
            spdlog::debug("No policies configured, allowing access");
            return true;
        }

        // All policies must pass (AND logic)
        for(const auto& policy : policies_)
        {
            if(!policy->Evaluate(claims, resource, action))
            {
                spdlog::debug("Policy evaluation failed for user {}", claims.user_id);
                return false;
            }
        }

        spdlog::debug("All {} policies passed for user {}", policies_.size(),
                      claims.user_id);
        return true;
    }

} // namespace auth
