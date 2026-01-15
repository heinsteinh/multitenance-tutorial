#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace auth
{

    // Forward declarations
    struct TokenClaims;
    struct Resource;

    /**
     * @brief Base class for authorization policies
     */
    class Policy
    {
    public:
        virtual ~Policy() = default;

        /**
         * @brief Evaluate if resource access is allowed
         * @param claims Token claims
         * @param resource Resource being accessed
         * @param action Action to perform
         * @return true if access allowed, false otherwise
         */
        virtual bool Evaluate(const TokenClaims& claims, const Resource& resource,
                              const std::string& action) = 0;
    };

    /**
     * @brief Ownership-based policy - only owner can modify
     */
    class OwnershipPolicy : public Policy
    {
    public:
        bool Evaluate(const TokenClaims& claims, const Resource& resource,
                      const std::string& action) override;
    };

    /**
     * @brief Tenant isolation policy - verify user belongs to resource tenant
     */
    class TenantIsolationPolicy : public Policy
    {
    public:
        bool Evaluate(const TokenClaims& claims, const Resource& resource,
                      const std::string& action) override;
    };

    /**
     * @brief Role-based policy - check user has required role
     */
    class RolePolicy : public Policy
    {
    public:
        explicit RolePolicy(std::vector<std::string> required_roles);

        bool Evaluate(const TokenClaims& claims, const Resource& resource,
                      const std::string& action) override;

    private:
        std::vector<std::string> required_roles_;
    };

    /**
     * @brief Policy engine for evaluating access control
     */
    class PolicyEngine
    {
    public:
        /**
         * @brief Add policy to evaluation chain
         */
        void AddPolicy(std::shared_ptr<Policy> policy);

        /**
         * @brief Clear all policies
         */
        void ClearPolicies();

        /**
         * @brief Evaluate all policies (AND logic - all must pass)
         * @param claims Token claims
         * @param resource Resource being accessed
         * @param action Action to perform
         * @return true if all policies allow, false otherwise
         */
        bool Evaluate(const TokenClaims& claims, const Resource& resource,
                      const std::string& action);

    private:
        std::vector<std::shared_ptr<Policy>> policies_;
    };

} // namespace auth
