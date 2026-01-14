/**
 * Step 04: Repository Pattern Demo
 *
 * Demonstrates:
 * - Entity definition
 * - Repository CRUD operations
 * - Specification pattern for queries
 * - Batch operations
 */

#include "pool/connection_pool.hpp"
#include "repository/user_repository.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>

using json = nlohmann::json;
using namespace repository;

void setup_logging() {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
}

void create_schema(db::Database &db) {
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
            updated_at TEXT,
            UNIQUE(tenant_id, username)
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
            created_at TEXT,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    )");

  db.execute("CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id)");
  db.execute("CREATE INDEX IF NOT EXISTS idx_permissions_user ON "
             "permissions(user_id)");
}

void demo_tenant_repository(pool::ConnectionPool &pool) {
  spdlog::info("=== Tenant Repository ===");

  TenantRepository repo(pool);

  // Insert tenants
  Tenant acme{.tenant_id = "acme-corp",
              .name = "ACME Corporation",
              .plan = "enterprise",
              .active = true,
              .db_path = "data/acme.db"};

  Tenant startup{.tenant_id = "cool-startup",
                 .name = "Cool Startup Inc",
                 .plan = "pro",
                 .active = true,
                 .db_path = "data/startup.db"};

  int64_t acme_id = repo.insert(acme);
  int64_t startup_id = repo.insert(startup);

  spdlog::info("Inserted tenants: ACME (ID={}), Startup (ID={})", acme_id,
               startup_id);

  // Find by tenant_id
  if (auto tenant = repo.find_by_tenant_id("acme-corp")) {
    spdlog::info("Found tenant: {} (plan={})", tenant->name, tenant->plan);
  }

  // List all active tenants
  auto active_tenants = repo.find_active();
  spdlog::info("Active tenants: {}", active_tenants.size());
  for (const auto &t : active_tenants) {
    spdlog::info("  - {} ({})", t.name, t.tenant_id);
  }
}

void demo_user_repository(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== User Repository ===");

  UserRepository repo(pool);

  // Insert users
  std::vector<User> users = {
      {.tenant_id = "acme-corp",
       .username = "alice",
       .email = "alice@acme.com",
       .password_hash = "hash1",
       .role = "admin",
       .active = true},
      {.tenant_id = "acme-corp",
       .username = "bob",
       .email = "bob@acme.com",
       .password_hash = "hash2",
       .role = "user",
       .active = true},
      {.tenant_id = "acme-corp",
       .username = "charlie",
       .email = "charlie@acme.com",
       .password_hash = "hash3",
       .role = "user",
       .active = false},
      {.tenant_id = "cool-startup",
       .username = "diana",
       .email = "diana@startup.io",
       .password_hash = "hash4",
       .role = "admin",
       .active = true},
  };

  auto ids = repo.insert_batch(users);
  spdlog::info("Inserted {} users", ids.size());

  // Find by ID
  if (auto user = repo.find_by_id(ids[0])) {
    spdlog::info("User ID {}: {} <{}>", user->id, user->username, user->email);
  }

  // Find by email
  if (auto user = repo.find_by_email("bob@acme.com")) {
    spdlog::info("Found by email: {}", user->username);
  }

  // Find by tenant
  auto acme_users = repo.find_by_tenant("acme-corp");
  spdlog::info("ACME users: {}", acme_users.size());

  // Find active by tenant
  auto active_users = repo.find_active_by_tenant("acme-corp");
  spdlog::info("Active ACME users: {}", active_users.size());

  // Count by tenant
  auto count = repo.count_by_tenant("acme-corp");
  spdlog::info("Total ACME users: {}", count);

  // Update user
  if (auto user = repo.find_by_username("acme-corp", "charlie")) {
    user->active = true;
    repo.update(*user);
    spdlog::info("Activated user: {}", user->username);
  }
}

void demo_specification_queries(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== Specification Queries ===");

  UserRepository repo(pool);

  // Complex query using specification
  Specification<User> spec;
  spec.where("tenant_id", "=", std::string("acme-corp"))
      .where("active", "=", static_cast<int64_t>(1))
      .order_by("username")
      .limit(10);

  auto results = repo.find_by(spec);
  spdlog::info("Active ACME users (limited to 10): {}", results.size());

  // LIKE query
  Specification<User> emailSpec;
  emailSpec.where_like("email", "%@acme.com");

  auto acme_emails = repo.find_by(emailSpec);
  spdlog::info("Users with @acme.com email: {}", acme_emails.size());

  // IN query
  Specification<User> roleSpec;
  roleSpec.where_in("role", std::vector<std::string>{"admin", "superuser"});

  auto admins = repo.find_by(roleSpec);
  spdlog::info("Admin/superuser users: {}", admins.size());
}

void demo_permission_repository(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== Permission Repository ===");

  UserRepository userRepo(pool);
  PermissionRepository permRepo(pool);

  // Get a user
  auto user = userRepo.find_by_username("acme-corp", "alice");
  if (!user) {
    spdlog::error("User not found");
    return;
  }

  // Grant permissions
  std::vector<Permission> perms = {
      {.tenant_id = "acme-corp",
       .user_id = user->id,
       .resource = "users",
       .action = "create"},
      {.tenant_id = "acme-corp",
       .user_id = user->id,
       .resource = "users",
       .action = "read"},
      {.tenant_id = "acme-corp",
       .user_id = user->id,
       .resource = "users",
       .action = "update"},
      {.tenant_id = "acme-corp",
       .user_id = user->id,
       .resource = "users",
       .action = "delete"},
      {.tenant_id = "acme-corp",
       .user_id = user->id,
       .resource = "reports",
       .action = "read"},
  };

  permRepo.insert_batch(perms);
  spdlog::info("Granted {} permissions to {}", perms.size(), user->username);

  // Check permissions
  bool can_create =
      permRepo.has_permission("acme-corp", user->id, "users", "create");
  bool can_delete_reports =
      permRepo.has_permission("acme-corp", user->id, "reports", "delete");

  spdlog::info("Alice can create users: {}", can_create ? "yes" : "no");
  spdlog::info("Alice can delete reports: {}",
               can_delete_reports ? "yes" : "no");

  // List user permissions
  auto user_perms = permRepo.find_by_user("acme-corp", user->id);
  spdlog::info("Alice's permissions:");
  for (const auto &p : user_perms) {
    spdlog::info("  - {}:{} = {}", p.resource, p.action,
                 p.allowed ? "allowed" : "denied");
  }
}

void demo_delete_operations(pool::ConnectionPool &pool) {
  spdlog::info("");
  spdlog::info("=== Delete Operations ===");

  UserRepository repo(pool);

  // Count before
  auto count_before = repo.count();
  spdlog::info("Users before delete: {}", count_before);

  // Delete inactive users
  Specification<User> inactiveSpec;
  inactiveSpec.where("active", "=", static_cast<int64_t>(0));

  size_t deleted = repo.remove_by(inactiveSpec);
  spdlog::info("Deleted {} inactive users", deleted);

  // Count after
  auto count_after = repo.count();
  spdlog::info("Users after delete: {}", count_after);
}

int main() {
  setup_logging();

  spdlog::info("╔════════════════════════════════════════════╗");
  spdlog::info("║  Step 04: Repository Pattern Demo          ║");
  spdlog::info("╚════════════════════════════════════════════╝");
  spdlog::info("");

  // Cleanup
  std::filesystem::remove("step04_demo.db");
  std::filesystem::remove("step04_demo.db-wal");
  std::filesystem::remove("step04_demo.db-shm");

  try {
    // Create pool
    pool::ConnectionPool pool(pool::PoolConfig{.db_path = "step04_demo.db",
                                               .min_connections = 2,
                                               .max_connections = 5});

    // Create schema
    {
      auto conn = pool.acquire();
      create_schema(*conn);
    }

    demo_tenant_repository(pool);
    demo_user_repository(pool);
    demo_specification_queries(pool);
    demo_permission_repository(pool);
    demo_delete_operations(pool);

    spdlog::info("");
    spdlog::info("=== Demo Complete ===");
    spdlog::info("Next: Step 05 - Tenant Management");

  } catch (const std::exception &e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }

  // Cleanup
  std::filesystem::remove("step04_demo.db");
  std::filesystem::remove("step04_demo.db-wal");
  std::filesystem::remove("step04_demo.db-shm");

  return 0;
}
