/**
 * Step 04: Repository Pattern Tests
 *
 * Comprehensive tests for the generic repository pattern, specifications,
 * and concrete entity repositories.
 */

#include <catch2/catch_test_macros.hpp>

#include "db/database.hpp"
#include "repository/user_repository.hpp"

using namespace repository;
using namespace db;

namespace {

/**
 * Helper function to create the test database schema
 */
void create_test_schema(Database &db) {
  db.execute(R"(
    CREATE TABLE IF NOT EXISTS tenants (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      tenant_id TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      plan TEXT DEFAULT 'free',
      active INTEGER DEFAULT 1,
      db_path TEXT,
      created_at TEXT,
      updated_at TEXT
    )
  )");

  db.execute(R"(
    CREATE TABLE IF NOT EXISTS users (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      tenant_id TEXT NOT NULL,
      username TEXT NOT NULL,
      email TEXT UNIQUE NOT NULL,
      password_hash TEXT,
      role TEXT DEFAULT 'user',
      active INTEGER DEFAULT 1,
      created_at TEXT,
      updated_at TEXT
    )
  )");

  db.execute(R"(
    CREATE TABLE IF NOT EXISTS permissions (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      tenant_id TEXT NOT NULL,
      user_id INTEGER NOT NULL,
      resource TEXT NOT NULL,
      action TEXT NOT NULL,
      allowed INTEGER DEFAULT 1,
      created_at TEXT
    )
  )");

  db.execute("CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id)");
  db.execute("CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)");
  db.execute(
      "CREATE INDEX IF NOT EXISTS idx_permissions_user ON permissions(user_id)");
}

} // namespace

// ==================== Entity Tests ====================

TEST_CASE("Entity static members are defined correctly", "[entity]") {
  SECTION("User entity") {
    REQUIRE(User::table_name == "users");
    REQUIRE(User::primary_key == "id");
  }

  SECTION("Tenant entity") {
    REQUIRE(Tenant::table_name == "tenants");
    REQUIRE(Tenant::primary_key == "id");
  }

  SECTION("Permission entity") {
    REQUIRE(Permission::table_name == "permissions");
    REQUIRE(Permission::primary_key == "id");
  }
}

// ==================== UserRepository CRUD Tests ====================

TEST_CASE("UserRepository insert and find", "[repository][user]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  SECTION("Insert returns valid ID") {
    User user{.tenant_id = "test-tenant",
              .username = "alice",
              .email = "alice@example.com",
              .password_hash = "hash123",
              .role = "admin",
              .active = true};

    auto id = repo.insert(user);
    REQUIRE(id > 0);
  }

  SECTION("Find by ID returns inserted user") {
    User user{.tenant_id = "test-tenant",
              .username = "bob",
              .email = "bob@example.com",
              .password_hash = "hash456",
              .role = "user",
              .active = true};

    auto id = repo.insert(user);
    auto found = repo.find_by_id(id);

    REQUIRE(found.has_value());
    REQUIRE(found->id == id);
    REQUIRE(found->username == "bob");
    REQUIRE(found->email == "bob@example.com");
    REQUIRE(found->role == "user");
    REQUIRE(found->active == true);
  }

  SECTION("Find by ID returns nullopt for non-existent") {
    auto found = repo.find_by_id(999);
    REQUIRE_FALSE(found.has_value());
  }

  SECTION("Find all returns all users") {
    repo.insert(User{.tenant_id = "t1",
                     .username = "u1",
                     .email = "u1@test.com",
                     .role = "user"});
    repo.insert(User{.tenant_id = "t1",
                     .username = "u2",
                     .email = "u2@test.com",
                     .role = "user"});
    repo.insert(User{.tenant_id = "t2",
                     .username = "u3",
                     .email = "u3@test.com",
                     .role = "admin"});

    auto all = repo.find_all();
    REQUIRE(all.size() == 3);
  }
}

TEST_CASE("UserRepository update", "[repository][user]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  User user{.tenant_id = "test-tenant",
            .username = "original",
            .email = "original@example.com",
            .password_hash = "hash",
            .role = "user",
            .active = true};

  auto id = repo.insert(user);

  SECTION("Update modifies existing user") {
    auto found = repo.find_by_id(id);
    REQUIRE(found.has_value());

    found->username = "updated";
    found->role = "admin";
    repo.update(*found);

    auto updated = repo.find_by_id(id);
    REQUIRE(updated.has_value());
    REQUIRE(updated->username == "updated");
    REQUIRE(updated->role == "admin");
    REQUIRE(updated->email == "original@example.com"); // Unchanged
  }
}

TEST_CASE("UserRepository remove", "[repository][user]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  User user{.tenant_id = "test-tenant",
            .username = "to_delete",
            .email = "delete@example.com",
            .role = "user"};

  auto id = repo.insert(user);

  SECTION("Remove deletes by ID") {
    REQUIRE(repo.find_by_id(id).has_value());

    repo.remove(id);

    REQUIRE_FALSE(repo.find_by_id(id).has_value());
  }

  SECTION("Remove_all clears table") {
    repo.insert(User{.tenant_id = "t1",
                     .username = "u1",
                     .email = "u1@test.com",
                     .role = "user"});
    repo.insert(User{.tenant_id = "t1",
                     .username = "u2",
                     .email = "u2@test.com",
                     .role = "user"});

    REQUIRE(repo.count() >= 2);

    auto removed = repo.remove_all();
    REQUIRE(removed >= 2);
    REQUIRE(repo.count() == 0);
  }
}

TEST_CASE("UserRepository batch insert", "[repository][user]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  std::vector<User> users = {
      {.tenant_id = "tenant1",
       .username = "batch1",
       .email = "batch1@test.com",
       .role = "user"},
      {.tenant_id = "tenant1",
       .username = "batch2",
       .email = "batch2@test.com",
       .role = "user"},
      {.tenant_id = "tenant1",
       .username = "batch3",
       .email = "batch3@test.com",
       .role = "admin"},
  };

  SECTION("Batch insert returns correct IDs") {
    auto ids = repo.insert_batch(users);

    REQUIRE(ids.size() == 3);
    REQUIRE(ids[0] > 0);
    REQUIRE(ids[1] > ids[0]);
    REQUIRE(ids[2] > ids[1]);
  }

  SECTION("Batch inserted users are retrievable") {
    auto ids = repo.insert_batch(users);

    for (size_t i = 0; i < ids.size(); ++i) {
      auto found = repo.find_by_id(ids[i]);
      REQUIRE(found.has_value());
      REQUIRE(found->username == users[i].username);
    }
  }

  SECTION("Empty batch returns empty vector") {
    auto ids = repo.insert_batch({});
    REQUIRE(ids.empty());
  }
}

// ==================== UserRepository Custom Methods Tests ====================

TEST_CASE("UserRepository find_by_email", "[repository][user]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  repo.insert(User{.tenant_id = "t1",
                   .username = "alice",
                   .email = "alice@example.com",
                   .role = "admin"});

  SECTION("Find by existing email") {
    auto found = repo.find_by_email("alice@example.com");
    REQUIRE(found.has_value());
    REQUIRE(found->username == "alice");
  }

  SECTION("Find by non-existent email returns nullopt") {
    auto found = repo.find_by_email("nobody@example.com");
    REQUIRE_FALSE(found.has_value());
  }
}

TEST_CASE("UserRepository tenant queries", "[repository][user]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  // Setup test data
  repo.insert(User{.tenant_id = "tenant-a",
                   .username = "alice",
                   .email = "alice@a.com",
                   .role = "admin",
                   .active = true});
  repo.insert(User{.tenant_id = "tenant-a",
                   .username = "bob",
                   .email = "bob@a.com",
                   .role = "user",
                   .active = true});
  repo.insert(User{.tenant_id = "tenant-a",
                   .username = "charlie",
                   .email = "charlie@a.com",
                   .role = "user",
                   .active = false});
  repo.insert(User{.tenant_id = "tenant-b",
                   .username = "dave",
                   .email = "dave@b.com",
                   .role = "user",
                   .active = true});

  SECTION("find_by_username finds user in tenant") {
    auto found = repo.find_by_username("tenant-a", "alice");
    REQUIRE(found.has_value());
    REQUIRE(found->email == "alice@a.com");
  }

  SECTION("find_by_username returns nullopt for wrong tenant") {
    auto found = repo.find_by_username("tenant-b", "alice");
    REQUIRE_FALSE(found.has_value());
  }

  SECTION("find_by_tenant returns all users in tenant") {
    auto users = repo.find_by_tenant("tenant-a");
    REQUIRE(users.size() == 3);
  }

  SECTION("find_active_by_tenant returns only active users") {
    auto users = repo.find_active_by_tenant("tenant-a");
    REQUIRE(users.size() == 2);
    for (const auto &u : users) {
      REQUIRE(u.active == true);
    }
  }

  SECTION("count_by_tenant returns correct count") {
    REQUIRE(repo.count_by_tenant("tenant-a") == 3);
    REQUIRE(repo.count_by_tenant("tenant-b") == 1);
    REQUIRE(repo.count_by_tenant("tenant-c") == 0);
  }
}

// ==================== Specification Tests ====================

TEST_CASE("Specification where clause", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  repo.insert(User{.tenant_id = "t1",
                   .username = "alice",
                   .email = "alice@test.com",
                   .role = "admin"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "bob",
                   .email = "bob@test.com",
                   .role = "user"});

  SECTION("Equality filter") {
    Specification<User> spec;
    spec.where("role", "=", std::string("admin"));

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].username == "alice");
  }

  SECTION("Multiple conditions (AND)") {
    Specification<User> spec;
    spec.where("tenant_id", "=", std::string("t1"))
        .where("role", "=", std::string("user"));

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].username == "bob");
  }
}

TEST_CASE("Specification null handling", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);

  db.execute(
      "INSERT INTO users (tenant_id, username, email, password_hash, role) "
      "VALUES ('t1', 'with_pass', 'wp@test.com', 'hash', 'user')");
  db.execute("INSERT INTO users (tenant_id, username, email, role) "
             "VALUES ('t1', 'no_pass', 'np@test.com', 'user')");

  UserRepository repo(db);

  SECTION("where_null finds NULL values") {
    Specification<User> spec;
    spec.where_null("password_hash");

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].username == "no_pass");
  }

  SECTION("where_not_null finds non-NULL values") {
    Specification<User> spec;
    spec.where_not_null("password_hash");

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].username == "with_pass");
  }
}

TEST_CASE("Specification in clause", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  auto id1 = repo.insert(User{.tenant_id = "t1",
                              .username = "alice",
                              .email = "alice@test.com",
                              .role = "admin"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "bob",
                   .email = "bob@test.com",
                   .role = "user"});
  auto id3 = repo.insert(User{.tenant_id = "t1",
                              .username = "charlie",
                              .email = "charlie@test.com",
                              .role = "user"});

  SECTION("where_in with int64_t vector") {
    Specification<User> spec;
    spec.where_in("id", std::vector<int64_t>{id1, id3});

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 2);
  }

  SECTION("where_in with string vector") {
    Specification<User> spec;
    spec.where_in("role", std::vector<std::string>{"admin", "moderator"});

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].role == "admin");
  }
}

TEST_CASE("Specification like clause", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  repo.insert(User{.tenant_id = "t1",
                   .username = "alice_smith",
                   .email = "alice@example.com",
                   .role = "user"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "alice_jones",
                   .email = "alice.jones@example.com",
                   .role = "user"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "bob",
                   .email = "bob@test.com",
                   .role = "user"});

  SECTION("LIKE with prefix pattern") {
    Specification<User> spec;
    spec.where_like("username", "alice%");

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 2);
  }

  SECTION("LIKE with contains pattern") {
    Specification<User> spec;
    spec.where_like("email", "%example%");

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 2);
  }
}

TEST_CASE("Specification ordering", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  repo.insert(User{.tenant_id = "t1",
                   .username = "charlie",
                   .email = "c@test.com",
                   .role = "user"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "alice",
                   .email = "a@test.com",
                   .role = "user"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "bob",
                   .email = "b@test.com",
                   .role = "user"});

  SECTION("Order by ascending") {
    Specification<User> spec;
    spec.order_by("username", SortOrder::Ascending);

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 3);
    REQUIRE(results[0].username == "alice");
    REQUIRE(results[1].username == "bob");
    REQUIRE(results[2].username == "charlie");
  }

  SECTION("Order by descending") {
    Specification<User> spec;
    spec.order_by_desc("username");

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 3);
    REQUIRE(results[0].username == "charlie");
    REQUIRE(results[1].username == "bob");
    REQUIRE(results[2].username == "alice");
  }
}

TEST_CASE("Specification pagination", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  for (int i = 1; i <= 10; ++i) {
    repo.insert(User{.tenant_id = "t1",
                     .username = "user" + std::to_string(i),
                     .email = "user" + std::to_string(i) + "@test.com",
                     .role = "user"});
  }

  SECTION("Limit restricts results") {
    Specification<User> spec;
    spec.limit(3);

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 3);
  }

  SECTION("Offset skips results") {
    Specification<User> spec;
    spec.order_by("username").offset(5).limit(3);

    auto results = repo.find_by(spec);
    REQUIRE(results.size() == 3);
    // With alphabetical ordering: user1, user10, user2, user3, user4, user5...
    // After offset 5: user5, user6, user7
  }
}

TEST_CASE("Specification composition", "[specification]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  repo.insert(User{.tenant_id = "t1",
                   .username = "alice",
                   .email = "alice@test.com",
                   .role = "admin",
                   .active = true});
  repo.insert(User{.tenant_id = "t1",
                   .username = "bob",
                   .email = "bob@test.com",
                   .role = "user",
                   .active = true});

  SECTION("and_spec combines specifications") {
    Specification<User> tenantSpec;
    tenantSpec.where("tenant_id", "=", std::string("t1"));

    Specification<User> roleSpec;
    roleSpec.where("role", "=", std::string("admin"));

    tenantSpec.and_spec(roleSpec);

    auto results = repo.find_by(tenantSpec);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].username == "alice");
  }
}

// ==================== TenantRepository Tests ====================

TEST_CASE("TenantRepository CRUD", "[repository][tenant]") {
  Database db(":memory:");
  create_test_schema(db);
  TenantRepository repo(db);

  SECTION("Insert and find tenant") {
    Tenant tenant{.tenant_id = "acme-corp",
                  .name = "ACME Corporation",
                  .plan = "enterprise",
                  .active = true,
                  .db_path = "/data/acme.db"};

    auto id = repo.insert(tenant);
    auto found = repo.find_by_id(id);

    REQUIRE(found.has_value());
    REQUIRE(found->tenant_id == "acme-corp");
    REQUIRE(found->name == "ACME Corporation");
    REQUIRE(found->plan == "enterprise");
  }
}

TEST_CASE("TenantRepository custom queries", "[repository][tenant]") {
  Database db(":memory:");
  create_test_schema(db);
  TenantRepository repo(db);

  repo.insert(Tenant{.tenant_id = "tenant-a",
                     .name = "Tenant A",
                     .plan = "free",
                     .active = true});
  repo.insert(Tenant{.tenant_id = "tenant-b",
                     .name = "Tenant B",
                     .plan = "pro",
                     .active = true});
  repo.insert(Tenant{.tenant_id = "tenant-c",
                     .name = "Tenant C",
                     .plan = "free",
                     .active = false});

  SECTION("find_by_tenant_id returns correct tenant") {
    auto found = repo.find_by_tenant_id("tenant-b");
    REQUIRE(found.has_value());
    REQUIRE(found->name == "Tenant B");
  }

  SECTION("find_active returns only active tenants") {
    auto active = repo.find_active();
    REQUIRE(active.size() == 2);
    for (const auto &t : active) {
      REQUIRE(t.active == true);
    }
  }

  SECTION("find_by_plan returns tenants with matching plan") {
    auto freeTenants = repo.find_by_plan("free");
    REQUIRE(freeTenants.size() == 2); // tenant-a and tenant-c (even inactive)
  }
}

// ==================== PermissionRepository Tests ====================

TEST_CASE("PermissionRepository queries", "[repository][permission]") {
  Database db(":memory:");
  create_test_schema(db);
  PermissionRepository repo(db);

  // Setup permissions
  repo.insert(Permission{.tenant_id = "t1",
                         .user_id = 1,
                         .resource = "users",
                         .action = "read",
                         .allowed = true});
  repo.insert(Permission{.tenant_id = "t1",
                         .user_id = 1,
                         .resource = "users",
                         .action = "write",
                         .allowed = true});
  repo.insert(Permission{.tenant_id = "t1",
                         .user_id = 2,
                         .resource = "users",
                         .action = "read",
                         .allowed = true});

  SECTION("find_by_user returns user permissions") {
    auto perms = repo.find_by_user("t1", 1);
    REQUIRE(perms.size() == 2);
  }
}

TEST_CASE("PermissionRepository has_permission", "[repository][permission]") {
  Database db(":memory:");
  create_test_schema(db);
  PermissionRepository repo(db);

  repo.insert(Permission{.tenant_id = "t1",
                         .user_id = 1,
                         .resource = "documents",
                         .action = "read",
                         .allowed = true});
  repo.insert(Permission{.tenant_id = "t1",
                         .user_id = 1,
                         .resource = "documents",
                         .action = "delete",
                         .allowed = false});

  SECTION("has_permission returns true for allowed permission") {
    REQUIRE(repo.has_permission("t1", 1, "documents", "read") == true);
  }

  SECTION("has_permission returns false for disallowed permission") {
    REQUIRE(repo.has_permission("t1", 1, "documents", "delete") == false);
  }

  SECTION("has_permission returns false for non-existent permission") {
    REQUIRE(repo.has_permission("t1", 1, "documents", "update") == false);
  }
}

// ==================== Edge Cases ====================

TEST_CASE("Empty results handling", "[repository]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  SECTION("find_all on empty table returns empty vector") {
    auto all = repo.find_all();
    REQUIRE(all.empty());
  }

  SECTION("find_by with no matches returns empty vector") {
    Specification<User> spec;
    spec.where("role", "=", std::string("nonexistent"));

    auto results = repo.find_by(spec);
    REQUIRE(results.empty());
  }

  SECTION("find_one with no matches returns nullopt") {
    Specification<User> spec;
    spec.where("id", "=", static_cast<int64_t>(999));

    auto result = repo.find_one(spec);
    REQUIRE_FALSE(result.has_value());
  }
}

TEST_CASE("Count operations", "[repository]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  SECTION("count on empty table returns 0") { REQUIRE(repo.count() == 0); }

  SECTION("count returns correct number after inserts") {
    repo.insert(User{.tenant_id = "t1",
                     .username = "u1",
                     .email = "u1@test.com",
                     .role = "user"});
    repo.insert(User{.tenant_id = "t1",
                     .username = "u2",
                     .email = "u2@test.com",
                     .role = "admin"});

    REQUIRE(repo.count() == 2);
  }

  SECTION("exists returns correct boolean") {
    Specification<User> spec;
    spec.where("role", "=", std::string("admin"));

    REQUIRE_FALSE(repo.exists(spec));

    repo.insert(User{.tenant_id = "t1",
                     .username = "admin",
                     .email = "admin@test.com",
                     .role = "admin"});

    REQUIRE(repo.exists(spec));
  }
}

TEST_CASE("remove_by with specification", "[repository]") {
  Database db(":memory:");
  create_test_schema(db);
  UserRepository repo(db);

  repo.insert(User{.tenant_id = "t1",
                   .username = "u1",
                   .email = "u1@test.com",
                   .role = "user"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "u2",
                   .email = "u2@test.com",
                   .role = "user"});
  repo.insert(User{.tenant_id = "t1",
                   .username = "u3",
                   .email = "u3@test.com",
                   .role = "admin"});

  SECTION("remove_by deletes matching entities") {
    Specification<User> spec;
    spec.where("role", "=", std::string("user"));

    auto removed = repo.remove_by(spec);
    REQUIRE(removed == 2);
    REQUIRE(repo.count() == 1);
  }
}
