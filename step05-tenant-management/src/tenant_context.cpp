#include "tenant/tenant_context.hpp"
#include <stdexcept>

namespace tenant {

// Thread-local storage
thread_local std::string TenantContext::current_tenant_id_;
thread_local int64_t TenantContext::current_user_id_ = 0;
thread_local bool TenantContext::has_context_ = false;

void TenantContext::set(const std::string& tenant_id, int64_t user_id) {
    current_tenant_id_ = tenant_id;
    current_user_id_ = user_id;
    has_context_ = true;
}

const std::string& TenantContext::tenant_id() {
    if (!has_context_) {
        throw std::runtime_error("No tenant context set");
    }
    return current_tenant_id_;
}

int64_t TenantContext::user_id() {
    return current_user_id_;
}

bool TenantContext::has_context() {
    return has_context_;
}

std::optional<std::string> TenantContext::try_get_tenant_id() {
    if (!has_context_) {
        return std::nullopt;
    }
    return current_tenant_id_;
}

void TenantContext::clear() {
    current_tenant_id_.clear();
    current_user_id_ = 0;
    has_context_ = false;
}

// ==================== TenantScope ====================

TenantScope::TenantScope(const std::string& tenant_id, int64_t user_id)
    : previous_tenant_id_(TenantContext::has_context() ? TenantContext::tenant_id() : "")
    , previous_user_id_(TenantContext::user_id())
    , had_previous_context_(TenantContext::has_context())
{
    TenantContext::set(tenant_id, user_id);
}

TenantScope::~TenantScope() {
    if (had_previous_context_) {
        TenantContext::set(previous_tenant_id_, previous_user_id_);
    } else {
        TenantContext::clear();
    }
}

} // namespace tenant
