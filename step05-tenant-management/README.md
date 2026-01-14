# Step 05: Tenant Management

Building on Step 04's repositories, this step implements multi-tenant database isolation with a database-per-tenant strategy.

## What's New in This Step

- Tenant database provisioning
- Database-per-tenant isolation
- Tenant context propagation
- Tenant lifecycle management (create, suspend, delete)
- Connection pool per tenant

## Multi-Tenancy Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       System Database                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   Tenants   │  │   Users     │  │   Global Config         │  │
│  │  - tenant1  │  │  - admins   │  │   - feature flags       │  │
│  │  - tenant2  │  │  - support  │  │   - system settings     │  │
│  │  - tenant3  │  │             │  │                         │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ TenantManager.get_database(tenant_id)
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Tenant Databases                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  tenant1.db  │  │  tenant2.db  │  │  tenant3.db  │          │
│  │  - orders    │  │  - orders    │  │  - orders    │          │
│  │  - products  │  │  - products  │  │  - products  │          │
│  │  - invoices  │  │  - invoices  │  │  - invoices  │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

## Key Concepts

### 1. Tenant Manager

Central component for tenant database access:

```cpp
class TenantManager {
public:
    // Get or create connection pool for tenant
    ConnectionPool& get_pool(const std::string& tenant_id);
    
    // Provision new tenant database
    void provision_tenant(const Tenant& tenant);
    
    // Remove tenant database
    void deprovision_tenant(const std::string& tenant_id);
    
    // Migrate all tenant databases
    void migrate_all(const Migration& migration);
};
```

### 2. Tenant Context

Thread-local context for request processing:

```cpp
class TenantContext {
    static thread_local std::string current_tenant_id_;
    static thread_local int64_t current_user_id_;
    
public:
    static void set(const std::string& tenant_id, int64_t user_id);
    static std::string tenant_id();
    static int64_t user_id();
    static void clear();
};

// RAII guard for context
class TenantScope {
public:
    TenantScope(const std::string& tenant_id, int64_t user_id);
    ~TenantScope();  // Restores previous context
};
```

### 3. Tenant-Aware Repository

Repositories that automatically use the correct tenant database:

```cpp
template<typename Entity>
class TenantRepository : public Repository<Entity> {
public:
    TenantRepository(TenantManager& manager)
        : manager_(manager) {}
    
protected:
    ConnectionPool& get_pool() override {
        return manager_.get_pool(TenantContext::tenant_id());
    }
};
```

## Project Structure

```
step05-tenant-management/
├── CMakeLists.txt
├── vcpkg.json
├── include/
│   └── tenant/
│       ├── tenant_manager.hpp    # Central tenant management
│       ├── tenant_context.hpp    # Thread-local context
│       ├── tenant_database.hpp   # Tenant DB operations
│       └── schema_migrator.hpp   # Schema migrations
├── src/
│   ├── tenant_manager.cpp
│   ├── tenant_context.cpp
│   └── main.cpp
└── tests/
    └── tenant_test.cpp
```

## Database Isolation Strategies

### Implemented: Database-per-Tenant

```
data/
├── system.db           # System database
├── tenants/
│   ├── acme.db         # ACME's isolated database
│   ├── startup.db      # Startup's isolated database
│   └── enterprise.db   # Enterprise's isolated database
```

**Pros:**
- Complete data isolation
- Easy backup/restore per tenant
- Independent scaling
- Simple compliance

**Cons:**
- More resource intensive
- Schema migrations across many databases
- Connection pool per tenant

### Alternative: Schema-per-Tenant (SQLite doesn't support)

### Alternative: Shared Schema with tenant_id

```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    tenant_id TEXT NOT NULL,  -- Partition key
    ...
);
CREATE INDEX idx_orders_tenant ON orders(tenant_id);
```

## Building

```bash
cmake --preset=default
cmake --build build
./build/step05_demo
```

## Usage Example

```cpp
// Initialize tenant manager
TenantManager manager("data/system.db", "data/tenants/");

// Provision new tenant
Tenant tenant{
    .tenant_id = "new-company",
    .name = "New Company Inc",
    .plan = "pro"
};
manager.provision_tenant(tenant);

// Use tenant database in request handler
void handle_request(const Request& req) {
    TenantScope scope(req.tenant_id, req.user_id);
    
    // All database operations now use correct tenant DB
    auto& pool = manager.get_pool(TenantContext::tenant_id());
    OrderRepository orders(pool);
    auto pending = orders.find_pending();
}
```

## Next Step

Step 06 introduces gRPC service definitions and code generation.
