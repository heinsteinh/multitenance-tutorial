# Step 04: Repository Pattern

Building on Step 03's connection pool, this step introduces the Repository pattern for type-safe data access.

## What's New in This Step

- Generic Repository interface
- Entity mapping with reflection-like macros
- Type-safe CRUD operations
- Query builder for complex queries
- Specification pattern for filtering

## Why Repository Pattern?

Without repositories:
```cpp
// Raw SQL scattered throughout code
conn->execute("SELECT id, name, email FROM users WHERE tenant_id = ?");
// Manual mapping everywhere
auto id = stmt.column<int>(0);
auto name = stmt.column<std::string>(1);
```

With repositories:
```cpp
// Clean, type-safe API
auto user = userRepo.find_by_id(123);
auto users = userRepo.find_all(UserSpec::active().and_tenant(tenant_id));
```

## Key Concepts

### 1. Entity Definition

```cpp
struct User {
    int64_t id = 0;
    std::string tenant_id;
    std::string name;
    std::string email;
    bool active = true;
    std::string created_at;

    // Entity metadata
    static constexpr auto table_name = "users";
    static constexpr auto primary_key = "id";
};

// Register fields for automatic mapping
REGISTER_ENTITY(User, id, tenant_id, name, email, active, created_at)
```

### 2. Repository Interface

```cpp
template<typename Entity>
class Repository {
public:
    // CRUD operations
    std::optional<Entity> find_by_id(int64_t id);
    std::vector<Entity> find_all();
    std::vector<Entity> find_by(const Specification<Entity>& spec);

    int64_t insert(const Entity& entity);
    void update(const Entity& entity);
    void remove(int64_t id);

    // Batch operations
    void insert_batch(const std::vector<Entity>& entities);
    int remove_by(const Specification<Entity>& spec);

    // Counting
    size_t count();
    size_t count_by(const Specification<Entity>& spec);
};
```

### 3. Specification Pattern

```cpp
// Build complex queries fluently
auto spec = UserSpec::builder()
    .where_tenant(tenant_id)
    .where_active(true)
    .order_by("created_at", Order::Desc)
    .limit(10)
    .build();

auto users = userRepo.find_by(spec);
```

## Project Structure

```
step04-repository-pattern/
├── CMakeLists.txt
├── vcpkg.json
├── include/
│   └── repository/
│       ├── entity.hpp          # Entity base and concept definition
│       ├── repository.hpp      # Generic repository template
│       ├── specification.hpp   # Specification pattern for queries
│       └── user_repository.hpp # Concrete entities and repositories
├── src/
│   └── main.cpp                # Demo application
└── tests/
    └── repository_test.cpp     # Comprehensive test suite
```

**Note:** The implementation is header-only. Entity mapping is done manually in
concrete repository classes (e.g., `UserRepository`, `TenantRepository`) since
C++ lacks runtime reflection. The Specification pattern provides fluent query
building capabilities.

## Entity Mapping

Each concrete repository implements mapping between C++ structs and database rows:

```cpp
// In UserRepository - manual mapping implementation
User map_from_row(db::Statement& stmt) const override {
    return User{
        .id = stmt.column<int64_t>(0),
        .tenant_id = stmt.column<std::string>(1),
        .username = stmt.column<std::string>(2),
        .email = stmt.column<std::string>(3),
        // ... remaining fields
    };
}

void bind_insert_values(db::Statement& stmt, const User& user) const override {
    stmt.bind(1, user.tenant_id);
    stmt.bind(2, user.username);
    stmt.bind(3, user.email);
    // ... remaining bindings
}
```

This approach provides type safety while working within C++'s lack of
runtime reflection.

## Building

```bash
cmake --preset=default
cmake --build build
./build/step04_demo
```

## Next Step

Step 05 introduces multi-tenant database management.
