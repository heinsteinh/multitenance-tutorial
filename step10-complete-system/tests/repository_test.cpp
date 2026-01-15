#include "db/database.hpp"
#include "db/schema_initializer.hpp"
#include "repository/tenant_repository.hpp"
#include "repository/user_repository.hpp"
#include "services/dto.hpp"

#include <catch2/catch_test_macros.hpp>

// Helper to create in-memory database with schema
std::shared_ptr<db::Database> create_test_database()
{
    db::DatabaseConfig config{
        .path               = ":memory:",
        .enable_foreign_keys = true,
        .enable_wal_mode    = false // WAL not supported for :memory:
    };
    auto database = std::make_shared<db::Database>(config);

    db::SchemaInitializer schema(database);
    schema.initialize_all();

    return database;
}

TEST_CASE("TenantRepository CRUD operations", "[repository][tenant]")
{
    auto database   = create_test_database();
    auto repository = repository::TenantRepository(database);

    SECTION("Insert and find tenant")
    {
        services::TenantModel tenant{
            .tenant_id = "test-tenant",
            .name      = "Test Tenant",
            .plan      = "enterprise",
            .active    = true,
        };

        auto id = repository.insert(tenant);
        REQUIRE(id > 0);

        auto found = repository.find_by_tenant_id("test-tenant");
        REQUIRE(found.has_value());
        REQUIRE(found->tenant_id == "test-tenant");
        REQUIRE(found->name == "Test Tenant");
        REQUIRE(found->plan == "enterprise");
        REQUIRE(found->active == true);
    }

    SECTION("Update tenant")
    {
        services::TenantModel tenant{
            .tenant_id = "update-test",
            .name      = "Original Name",
            .plan      = "basic",
            .active    = true,
        };

        auto id = repository.insert(tenant);

        auto found     = repository.find_by_tenant_id("update-test");
        found->name    = "Updated Name";
        found->plan    = "pro";
        repository.update(*found);

        auto updated = repository.find_by_tenant_id("update-test");
        REQUIRE(updated->name == "Updated Name");
        REQUIRE(updated->plan == "pro");
    }

    SECTION("Find all tenants")
    {
        services::TenantModel tenant1{.tenant_id = "tenant1", .name = "Tenant 1"};
        services::TenantModel tenant2{.tenant_id = "tenant2", .name = "Tenant 2"};

        repository.insert(tenant1);
        repository.insert(tenant2);

        auto all = repository.find_all();
        REQUIRE(all.size() >= 2);
    }

    SECTION("Tenant ID exists check")
    {
        services::TenantModel tenant{.tenant_id = "exists-test", .name = "Exists Test"};
        repository.insert(tenant);

        REQUIRE(repository.tenant_id_exists("exists-test") == true);
        REQUIRE(repository.tenant_id_exists("nonexistent") == false);
    }

    SECTION("Activate and deactivate tenant")
    {
        services::TenantModel tenant{
            .tenant_id = "active-test",
            .name      = "Active Test",
            .active    = true,
        };
        repository.insert(tenant);

        REQUIRE(repository.is_active("active-test") == true);

        repository.deactivate("active-test");
        REQUIRE(repository.is_active("active-test") == false);

        repository.activate("active-test");
        REQUIRE(repository.is_active("active-test") == true);
    }
}

TEST_CASE("UserRepository CRUD operations", "[repository][user]")
{
    auto database   = create_test_database();
    auto repository = repository::UserRepository(database);

    // First create a tenant for the user
    auto tenant_repo = repository::TenantRepository(database);
    services::TenantModel tenant{.tenant_id = "user-test-tenant", .name = "User Test"};
    tenant_repo.insert(tenant);

    SECTION("Insert and find user")
    {
        services::UserModel user{
            .tenant_id     = "user-test-tenant",
            .username      = "testuser",
            .email         = "test@example.com",
            .password_hash = "hashed_password",
            .role          = "admin",
            .active        = true,
        };

        auto id = repository.insert(user);
        REQUIRE(id > 0);

        auto found = repository.find_by_id(id);
        REQUIRE(found.has_value());
        REQUIRE(found->username == "testuser");
        REQUIRE(found->email == "test@example.com");
        REQUIRE(found->tenant_id == "user-test-tenant");
    }

    SECTION("Find user by email")
    {
        services::UserModel user{
            .tenant_id = "user-test-tenant",
            .username  = "emailuser",
            .email     = "unique@example.com",
            .role      = "user",
        };
        repository.insert(user);

        auto found = repository.find_by_email("unique@example.com");
        REQUIRE(found.has_value());
        REQUIRE(found->username == "emailuser");
    }

    SECTION("Find user by username within tenant")
    {
        services::UserModel user{
            .tenant_id = "user-test-tenant",
            .username  = "findbyname",
            .email     = "findbyname@example.com",
        };
        repository.insert(user);

        auto found = repository.find_by_username("user-test-tenant", "findbyname");
        REQUIRE(found.has_value());
        REQUIRE(found->email == "findbyname@example.com");

        auto not_found = repository.find_by_username("other-tenant", "findbyname");
        REQUIRE_FALSE(not_found.has_value());
    }

    SECTION("Update user")
    {
        services::UserModel user{
            .tenant_id = "user-test-tenant",
            .username  = "updateuser",
            .email     = "update@example.com",
            .role      = "user",
        };
        auto id = repository.insert(user);

        auto found  = repository.find_by_id(id);
        found->role = "admin";
        repository.update(*found);

        auto updated = repository.find_by_id(id);
        REQUIRE(updated->role == "admin");
    }

    SECTION("Find users by tenant")
    {
        services::UserModel user1{
            .tenant_id = "user-test-tenant",
            .username  = "tenant-user-1",
            .email     = "tenant1@example.com",
        };
        services::UserModel user2{
            .tenant_id = "user-test-tenant",
            .username  = "tenant-user-2",
            .email     = "tenant2@example.com",
        };
        repository.insert(user1);
        repository.insert(user2);

        auto users = repository.find_by_tenant("user-test-tenant");
        REQUIRE(users.size() >= 2);
    }

    SECTION("Email exists check")
    {
        services::UserModel user{
            .tenant_id = "user-test-tenant",
            .username  = "emailcheck",
            .email     = "emailcheck@example.com",
        };
        repository.insert(user);

        REQUIRE(repository.email_exists("emailcheck@example.com") == true);
        REQUIRE(repository.email_exists("nonexistent@example.com") == false);
    }

    SECTION("Username exists within tenant")
    {
        services::UserModel user{
            .tenant_id = "user-test-tenant",
            .username  = "usernamecheck",
            .email     = "usernamecheck@example.com",
        };
        repository.insert(user);

        REQUIRE(repository.username_exists("user-test-tenant", "usernamecheck") == true);
        REQUIRE(repository.username_exists("user-test-tenant", "nonexistent") == false);
        REQUIRE(repository.username_exists("other-tenant", "usernamecheck") == false);
    }
}
