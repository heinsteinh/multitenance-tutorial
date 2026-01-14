# Step 09: Authorization

This step implements a complete Role-Based Access Control (RBAC) system with SQLite-backed permissions.

## What's New in This Step

- Role hierarchy and management
- Permission checking
- Resource-based authorization
- Policy enforcement
- JWT token validation

## RBAC Model

```
┌─────────────────────────────────────────────────────────────┐
│                        RBAC Model                           │
│                                                             │
│  ┌─────────┐    has     ┌─────────┐   grants   ┌─────────┐ │
│  │  User   │ ─────────▶ │  Role   │ ─────────▶ │ Permis- │ │
│  │         │            │         │            │  sion   │ │
│  └─────────┘            └─────────┘            └─────────┘ │
│       │                      │                      │       │
│       │                      │                      │       │
│       ▼                      ▼                      ▼       │
│  ┌─────────┐            ┌─────────┐            ┌─────────┐ │
│  │ alice   │            │ admin   │            │ users:  │ │
│  │ bob     │            │ editor  │            │  create │ │
│  │ charlie │            │ viewer  │            │  read   │ │
│  └─────────┘            └─────────┘            │  update │ │
│                                                │  delete │ │
│                                                └─────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Key Concepts

### 1. Permission Structure

```cpp
struct Permission {
    std::string resource;  // e.g., "users", "orders", "reports"
    std::string action;    // e.g., "create", "read", "update", "delete"
};

struct Role {
    std::string name;
    std::vector<Permission> permissions;
    std::optional<std::string> parent;  // For role hierarchy
};
```

### 2. Authorization Service

```cpp
class AuthorizationService {
public:
    // Check if user has permission
    bool has_permission(int64_t user_id,
                       const std::string& resource,
                       const std::string& action);

    // Check with resource ownership
    bool can_access(int64_t user_id,
                   const std::string& resource,
                   const std::string& action,
                   int64_t resource_owner_id);

    // Grant/revoke permissions
    void grant_role(int64_t user_id, const std::string& role);
    void revoke_role(int64_t user_id, const std::string& role);

    // Get user's effective permissions (including inherited)
    std::vector<Permission> get_effective_permissions(int64_t user_id);
};
```

### 3. Authorization Decorator

```cpp
// Attribute-style authorization
class Authorize {
public:
    Authorize(const std::string& resource, const std::string& action)
        : resource_(resource), action_(action) {}

    template<typename Func>
    auto operator()(Func&& handler) {
        return [=](auto&&... args) {
            auto user_id = TenantContext::user_id();

            if (!auth_service_.has_permission(user_id, resource_, action_)) {
                throw AuthorizationException("Access denied");
            }

            return handler(std::forward<decltype(args)>(args)...);
        };
    }
};

// Usage in handler
Status CreateUser(...) {
    return Authorize("users", "create")([&]() {
        // Handler logic
        return Status::OK;
    });
}
```

### 4. Policy-Based Authorization

```cpp
class Policy {
public:
    virtual bool evaluate(const AuthContext& ctx,
                         const Resource& resource) = 0;
};

class OwnershipPolicy : public Policy {
public:
    bool evaluate(const AuthContext& ctx, const Resource& resource) override {
        return resource.owner_id == ctx.user_id;
    }
};

class TenantIsolationPolicy : public Policy {
public:
    bool evaluate(const AuthContext& ctx, const Resource& resource) override {
        return resource.tenant_id == ctx.tenant_id;
    }
};

class PolicyEngine {
public:
    bool authorize(const AuthContext& ctx,
                  const Resource& resource,
                  const std::string& action) {
        // Check explicit permissions first
        if (has_explicit_permission(ctx, resource, action)) {
            return true;
        }

        // Evaluate policies
        for (const auto& policy : policies_) {
            if (!policy->evaluate(ctx, resource)) {
                return false;
            }
        }

        return true;
    }
};
```

### 5. JWT Token Handling

```cpp
class JwtValidator {
public:
    struct Claims {
        int64_t user_id;
        std::string tenant_id;
        std::vector<std::string> roles;
        int64_t expires_at;
    };

    Claims validate(const std::string& token) {
        // Verify signature
        auto decoded = jwt::decode(token);

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256(secret_))
            .with_issuer("multitenant-service");

        verifier.verify(decoded);

        // Check expiration
        auto exp = decoded.get_payload_claim("exp").as_int();
        if (exp < std::time(nullptr)) {
            throw AuthException("Token expired");
        }

        return Claims{
            .user_id = decoded.get_payload_claim("sub").as_int(),
            .tenant_id = decoded.get_payload_claim("tenant").as_string(),
            .roles = decoded.get_payload_claim("roles").as_array(),
            .expires_at = exp
        };
    }

    std::string generate(const Claims& claims) {
        return jwt::create()
            .set_issuer("multitenant-service")
            .set_type("JWT")
            .set_payload_claim("sub", jwt::claim(claims.user_id))
            .set_payload_claim("tenant", jwt::claim(claims.tenant_id))
            .set_payload_claim("roles", jwt::claim(claims.roles))
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24))
            .sign(jwt::algorithm::hs256(secret_));
    }
};
```

## Database Schema

```sql
-- Roles table
CREATE TABLE roles (
    id INTEGER PRIMARY KEY,
    tenant_id TEXT NOT NULL,
    name TEXT NOT NULL,
    parent_role TEXT,
    UNIQUE(tenant_id, name)
);

-- Role permissions
CREATE TABLE role_permissions (
    id INTEGER PRIMARY KEY,
    role_id INTEGER NOT NULL,
    resource TEXT NOT NULL,
    action TEXT NOT NULL,
    FOREIGN KEY (role_id) REFERENCES roles(id),
    UNIQUE(role_id, resource, action)
);

-- User roles
CREATE TABLE user_roles (
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id),
    FOREIGN KEY (role_id) REFERENCES roles(id)
);

-- Direct user permissions (overrides)
CREATE TABLE user_permissions (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    resource TEXT NOT NULL,
    action TEXT NOT NULL,
    allowed INTEGER DEFAULT 1
);
```

## Project Structure

```
step09-authorization/
├── include/
│   └── auth/
│       ├── authorization_service.hpp
│       ├── jwt_validator.hpp
│       ├── policy_engine.hpp
│       ├── role_repository.hpp
│       └── permission_guard.hpp
├── src/
│   ├── authorization_service.cpp
│   ├── jwt_validator.cpp
│   └── main.cpp
```

## Next Step

Step 10 brings everything together into a complete, production-ready system.
