#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace tenant {

/**
 * Thread-local context for the current tenant and user.
 * 
 * Used to propagate tenant identity through the request processing chain.
 * Set at the start of request handling, cleared at the end.
 */
class TenantContext {
public:
    /**
     * Set the current tenant and user context
     */
    static void set(const std::string& tenant_id, int64_t user_id = 0);
    
    /**
     * Get the current tenant ID
     * @throws std::runtime_error if no context is set
     */
    static const std::string& tenant_id();
    
    /**
     * Get the current user ID
     */
    static int64_t user_id();
    
    /**
     * Check if a tenant context is set
     */
    static bool has_context();
    
    /**
     * Get tenant ID if set, nullopt otherwise
     */
    static std::optional<std::string> try_get_tenant_id();
    
    /**
     * Clear the current context
     */
    static void clear();

private:
    static thread_local std::string current_tenant_id_;
    static thread_local int64_t current_user_id_;
    static thread_local bool has_context_;
};

/**
 * RAII guard for tenant context.
 * 
 * Sets context on construction, restores previous context on destruction.
 * 
 * Usage:
 *   void handle_request(const std::string& tenant_id) {
 *       TenantScope scope(tenant_id, user_id);
 *       // All operations in this scope use the specified tenant
 *   }  // Previous context restored here
 */
class TenantScope {
public:
    /**
     * Create a scope with the specified tenant context
     */
    TenantScope(const std::string& tenant_id, int64_t user_id = 0);
    
    ~TenantScope();
    
    // Non-copyable, non-movable
    TenantScope(const TenantScope&) = delete;
    TenantScope& operator=(const TenantScope&) = delete;
    TenantScope(TenantScope&&) = delete;
    TenantScope& operator=(TenantScope&&) = delete;

private:
    std::string previous_tenant_id_;
    int64_t previous_user_id_;
    bool had_previous_context_;
};

} // namespace tenant
