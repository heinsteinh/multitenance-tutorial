/**
 * Step 06: gRPC Server Implementation
 *
 * Basic gRPC server demonstrating:
 * - Service implementation
 * - Metadata extraction for tenant context
 * - Integration with repositories
 */

#include "repository/user_repository.hpp"
#include "tenant.grpc.pb.h"
#include "tenant/tenant_context.hpp"
#include "tenant/tenant_manager.hpp"
#include "user.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

namespace {

/**
 * Extract tenant ID from gRPC metadata
 */
std::string get_tenant_id(ServerContext *context) {
  auto metadata = context->client_metadata();
  auto it = metadata.find("x-tenant-id");
  if (it != metadata.end()) {
    return std::string(it->second.data(), it->second.size());
  }
  return "";
}

/**
 * Extract user ID from gRPC metadata
 */
int64_t get_user_id(ServerContext *context) {
  auto metadata = context->client_metadata();
  auto it = metadata.find("x-user-id");
  if (it != metadata.end()) {
    std::string value(it->second.data(), it->second.size());
    return std::stoll(value);
  }
  return 0;
}

/**
 * Helper: Populate tenant proto from repository tenant
 */
void populate_tenant(multitenant::v1::Tenant *proto,
                     const repository::Tenant &tenant) {
  proto->set_id(tenant.id);
  proto->set_tenant_id(tenant.tenant_id);
  proto->set_name(tenant.name);
  proto->set_plan(tenant.plan);
  proto->set_active(tenant.active);
  proto->set_created_at(tenant.created_at);
  proto->set_updated_at(tenant.updated_at);
}

/**
 * Helper: Populate user proto from database statement
 */
void populate_user_from_stmt(multitenant::v1::User *user, db::Statement &stmt) {
  user->set_id(stmt.column<int64_t>(0));
  user->set_username(stmt.column<std::string>(1));
  user->set_email(stmt.column<std::string>(2));
  user->set_role(stmt.column<std::string>(3));
  user->set_active(stmt.column<int64_t>(4) != 0);
  if (stmt.column_count() > 5) {
    user->set_created_at(stmt.column<std::string>(5));
  }
  if (stmt.column_count() > 6) {
    user->set_updated_at(stmt.column<std::string>(6));
  }
}

/**
 * Generate a simple token (for demo purposes - use JWT in production)
 */
std::string generate_simple_token(int64_t user_id, const std::string &username) {
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                       now.time_since_epoch())
                       .count();
  return std::to_string(user_id) + ":" + username + ":" +
         std::to_string(timestamp);
}

} // namespace

/**
 * Tenant service implementation
 */
class TenantServiceImpl final : public multitenant::v1::TenantService::Service {
public:
  explicit TenantServiceImpl(tenant::TenantManager &manager)
      : manager_(manager) {}

  Status GetTenant(ServerContext *context,
                   const multitenant::v1::GetTenantRequest *request,
                   multitenant::v1::GetTenantResponse *response) override {
    spdlog::info("GetTenant: {}", request->tenant_id());

    auto tenant_opt = manager_.get_tenant(request->tenant_id());
    if (!tenant_opt) {
      return Status(StatusCode::NOT_FOUND, "Tenant not found");
    }

    auto *tenant = response->mutable_tenant();
    tenant->set_id(tenant_opt->id);
    tenant->set_tenant_id(tenant_opt->tenant_id);
    tenant->set_name(tenant_opt->name);
    tenant->set_plan(tenant_opt->plan);
    tenant->set_active(tenant_opt->active);
    tenant->set_created_at(tenant_opt->created_at);
    tenant->set_updated_at(tenant_opt->updated_at);

    return Status::OK;
  }

  Status ListTenants(ServerContext *context,
                     const multitenant::v1::ListTenantsRequest *request,
                     multitenant::v1::ListTenantsResponse *response) override {
    spdlog::info("ListTenants");

    auto tenant_ids = manager_.get_active_tenant_ids();

    for (const auto &id : tenant_ids) {
      if (auto tenant_opt = manager_.get_tenant(id)) {
        auto *tenant = response->add_tenants();
        tenant->set_id(tenant_opt->id);
        tenant->set_tenant_id(tenant_opt->tenant_id);
        tenant->set_name(tenant_opt->name);
        tenant->set_plan(tenant_opt->plan);
        tenant->set_active(tenant_opt->active);
      }
    }

    auto *pagination = response->mutable_pagination();
    pagination->set_page(1);
    pagination->set_page_size(static_cast<int>(tenant_ids.size()));
    pagination->set_total_items(static_cast<int>(tenant_ids.size()));
    pagination->set_total_pages(1);

    return Status::OK;
  }

  Status
  CreateTenant(ServerContext *context,
               const multitenant::v1::CreateTenantRequest *request,
               multitenant::v1::CreateTenantResponse *response) override {
    spdlog::info("CreateTenant: {}", request->tenant_id());

    try {
      repository::Tenant tenant{
          .tenant_id = request->tenant_id(),
          .name = request->name(),
          .plan = request->plan().empty() ? "free" : request->plan(),
          .active = true};

      manager_.provision_tenant(tenant);

      // Fetch the created tenant
      if (auto created = manager_.get_tenant(request->tenant_id())) {
        auto *t = response->mutable_tenant();
        t->set_id(created->id);
        t->set_tenant_id(created->tenant_id);
        t->set_name(created->name);
        t->set_plan(created->plan);
        t->set_active(created->active);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status
  DeleteTenant(ServerContext *context,
               const multitenant::v1::DeleteTenantRequest *request,
               multitenant::v1::DeleteTenantResponse *response) override {
    spdlog::info("DeleteTenant: {} (permanent={})", request->tenant_id(),
                 request->permanent());

    try {
      manager_.deprovision_tenant(request->tenant_id(), request->permanent());
      response->set_success(true);
      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status
  UpdateTenant(ServerContext *context,
               const multitenant::v1::UpdateTenantRequest *request,
               multitenant::v1::UpdateTenantResponse *response) override {
    spdlog::info("UpdateTenant: {}", request->tenant_id());

    try {
      // Get existing tenant
      auto tenant_opt = manager_.get_tenant(request->tenant_id());
      if (!tenant_opt) {
        return Status(StatusCode::NOT_FOUND, "Tenant not found");
      }

      // Build update query based on provided fields
      auto &system_pool = manager_.get_system_pool();
      auto conn = system_pool.acquire();

      std::vector<std::string> updates;
      std::vector<std::variant<std::string, int64_t>> values;

      if (request->has_name()) {
        updates.push_back("name = ?");
        values.push_back(request->name());
      }
      if (request->has_plan()) {
        updates.push_back("plan = ?");
        values.push_back(request->plan());
      }
      if (request->has_active()) {
        updates.push_back("active = ?");
        values.push_back(request->active() ? 1LL : 0LL);
      }

      if (updates.empty()) {
        // Nothing to update, return current tenant
        populate_tenant(response->mutable_tenant(), *tenant_opt);
        return Status::OK;
      }

      updates.push_back("updated_at = datetime('now')");

      std::string sql = "UPDATE tenants SET ";
      for (size_t i = 0; i < updates.size(); ++i) {
        if (i > 0)
          sql += ", ";
        sql += updates[i];
      }
      sql += " WHERE tenant_id = ?";

      auto stmt = conn->prepare(sql);
      int idx = 1;
      for (const auto &val : values) {
        if (std::holds_alternative<std::string>(val)) {
          stmt.bind(idx++, std::get<std::string>(val));
        } else {
          stmt.bind(idx++, std::get<int64_t>(val));
        }
      }
      stmt.bind(idx, request->tenant_id());
      stmt.step();

      // Fetch updated tenant
      if (auto updated = manager_.get_tenant(request->tenant_id())) {
        populate_tenant(response->mutable_tenant(), *updated);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status ProvisionTenant(
      ServerContext *context,
      const multitenant::v1::ProvisionTenantRequest *request,
      multitenant::v1::ProvisionTenantResponse *response) override {
    spdlog::info("ProvisionTenant: {}", request->tenant_id());

    try {
      // Check if tenant exists
      auto tenant_opt = manager_.get_tenant(request->tenant_id());
      if (!tenant_opt) {
        return Status(StatusCode::NOT_FOUND, "Tenant not found in system");
      }

      // Provision the database (may already exist)
      manager_.provision_tenant(*tenant_opt);

      response->set_success(true);
      response->set_database_path(tenant_opt->db_path);

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

private:
  tenant::TenantManager &manager_;
};

/**
 * User service implementation
 */
class UserServiceImpl final : public multitenant::v1::UserService::Service {
public:
  explicit UserServiceImpl(tenant::TenantManager &manager)
      : manager_(manager) {}

  Status GetUser(ServerContext *context,
                 const multitenant::v1::GetUserRequest *request,
                 multitenant::v1::GetUserResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("GetUser: {} (tenant={})", request->user_id(), tenant_id);

    try {
      tenant::TenantScope scope(tenant_id);
      auto &pool = manager_.get_pool(tenant_id);

      // Query user from tenant database
      auto conn = pool.acquire();
      auto stmt =
          conn->prepare("SELECT id, username, email, role, active, created_at, "
                        "updated_at FROM users WHERE id = ?");
      stmt.bind(1, request->user_id());

      if (stmt.step()) {
        auto *user = response->mutable_user();
        user->set_id(stmt.column<int64_t>(0));
        user->set_username(stmt.column<std::string>(1));
        user->set_email(stmt.column<std::string>(2));
        user->set_role(stmt.column<std::string>(3));
        user->set_active(stmt.column<int64_t>(4) != 0);
        user->set_created_at(stmt.column<std::string>(5));
        user->set_updated_at(stmt.column<std::string>(6));
        return Status::OK;
      }

      return Status(StatusCode::NOT_FOUND, "User not found");
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status ListUsers(ServerContext *context,
                   const multitenant::v1::ListUsersRequest *request,
                   multitenant::v1::ListUsersResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("ListUsers (tenant={})", tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      std::string sql = "SELECT id, username, email, role, active, created_at, "
                        "updated_at FROM users";
      if (request->active_only()) {
        sql += " WHERE active = 1";
      }
      sql += " ORDER BY username";

      auto stmt = conn->prepare(sql);

      while (stmt.step()) {
        auto *user = response->add_users();
        user->set_id(stmt.column<int64_t>(0));
        user->set_username(stmt.column<std::string>(1));
        user->set_email(stmt.column<std::string>(2));
        user->set_role(stmt.column<std::string>(3));
        user->set_active(stmt.column<int64_t>(4) != 0);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status CreateUser(ServerContext *context,
                    const multitenant::v1::CreateUserRequest *request,
                    multitenant::v1::CreateUserResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("CreateUser: {} (tenant={})", request->username(), tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      auto stmt = conn->prepare(R"(
                INSERT INTO users (username, email, password_hash, role, active, created_at, updated_at)
                VALUES (?, ?, ?, ?, 1, datetime('now'), datetime('now'))
            )");

      stmt.bind(1, request->username());
      stmt.bind(2, request->email());
      stmt.bind(3, request->password()); // In production, hash this!
      stmt.bind(4, request->role().empty() ? "user" : request->role());
      stmt.step();

      int64_t user_id = conn->last_insert_rowid();

      auto *user = response->mutable_user();
      user->set_id(user_id);
      user->set_username(request->username());
      user->set_email(request->email());
      user->set_role(request->role().empty() ? "user" : request->role());
      user->set_active(true);

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status GetUserByUsername(
      ServerContext *context,
      const multitenant::v1::GetUserByUsernameRequest *request,
      multitenant::v1::GetUserResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("GetUserByUsername: {} (tenant={})", request->username(),
                 tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      auto stmt = conn->prepare(
          "SELECT id, username, email, role, active, created_at, updated_at "
          "FROM users WHERE username = ?");
      stmt.bind(1, request->username());

      if (stmt.step()) {
        populate_user_from_stmt(response->mutable_user(), stmt);
        return Status::OK;
      }

      return Status(StatusCode::NOT_FOUND, "User not found");
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status UpdateUser(ServerContext *context,
                    const multitenant::v1::UpdateUserRequest *request,
                    multitenant::v1::UpdateUserResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("UpdateUser: {} (tenant={})", request->user_id(), tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      // Check if user exists
      auto check_stmt = conn->prepare("SELECT id FROM users WHERE id = ?");
      check_stmt.bind(1, request->user_id());
      if (!check_stmt.step()) {
        return Status(StatusCode::NOT_FOUND, "User not found");
      }

      // Build dynamic update
      std::vector<std::string> updates;
      std::vector<std::variant<std::string, int64_t>> values;

      if (request->has_username()) {
        updates.push_back("username = ?");
        values.push_back(request->username());
      }
      if (request->has_email()) {
        updates.push_back("email = ?");
        values.push_back(request->email());
      }
      if (request->has_password()) {
        updates.push_back("password_hash = ?");
        values.push_back(request->password()); // In production, hash this!
      }
      if (request->has_role()) {
        updates.push_back("role = ?");
        values.push_back(request->role());
      }
      if (request->has_active()) {
        updates.push_back("active = ?");
        values.push_back(request->active() ? 1LL : 0LL);
      }

      if (!updates.empty()) {
        updates.push_back("updated_at = datetime('now')");

        std::string sql = "UPDATE users SET ";
        for (size_t i = 0; i < updates.size(); ++i) {
          if (i > 0)
            sql += ", ";
          sql += updates[i];
        }
        sql += " WHERE id = ?";

        auto stmt = conn->prepare(sql);
        int idx = 1;
        for (const auto &val : values) {
          if (std::holds_alternative<std::string>(val)) {
            stmt.bind(idx++, std::get<std::string>(val));
          } else {
            stmt.bind(idx++, std::get<int64_t>(val));
          }
        }
        stmt.bind(idx, request->user_id());
        stmt.step();
      }

      // Fetch updated user
      auto fetch_stmt = conn->prepare(
          "SELECT id, username, email, role, active, created_at, updated_at "
          "FROM users WHERE id = ?");
      fetch_stmt.bind(1, request->user_id());
      if (fetch_stmt.step()) {
        populate_user_from_stmt(response->mutable_user(), fetch_stmt);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status DeleteUser(ServerContext *context,
                    const multitenant::v1::DeleteUserRequest *request,
                    multitenant::v1::DeleteUserResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("DeleteUser: {} (tenant={}, permanent={})", request->user_id(),
                 tenant_id, request->permanent());

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      if (request->permanent()) {
        // Hard delete
        auto stmt = conn->prepare("DELETE FROM users WHERE id = ?");
        stmt.bind(1, request->user_id());
        stmt.step();
      } else {
        // Soft delete (deactivate)
        auto stmt = conn->prepare(
            "UPDATE users SET active = 0, updated_at = datetime('now') "
            "WHERE id = ?");
        stmt.bind(1, request->user_id());
        stmt.step();
      }

      response->set_success(true);
      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status Authenticate(ServerContext *context,
                      const multitenant::v1::AuthenticateRequest *request,
                      multitenant::v1::AuthenticateResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("Authenticate: {} (tenant={})", request->username(),
                 tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      auto stmt = conn->prepare(
          "SELECT id, username, email, role, active, password_hash, "
          "created_at, updated_at FROM users WHERE username = ? AND active = 1");
      stmt.bind(1, request->username());

      if (!stmt.step()) {
        response->set_success(false);
        return Status::OK;
      }

      int64_t user_id = stmt.column<int64_t>(0);
      std::string stored_password = stmt.column<std::string>(5);

      // Simple password check (in production, use proper hashing!)
      if (stored_password != request->password()) {
        response->set_success(false);
        return Status::OK;
      }

      // Authentication successful
      response->set_success(true);

      auto *user = response->mutable_user();
      user->set_id(user_id);
      user->set_username(stmt.column<std::string>(1));
      user->set_email(stmt.column<std::string>(2));
      user->set_role(stmt.column<std::string>(3));
      user->set_active(stmt.column<int64_t>(4) != 0);

      // Generate simple token
      response->set_token(generate_simple_token(user_id, request->username()));

      // Token expires in 1 hour
      auto expiry = std::chrono::system_clock::now() + std::chrono::hours(1);
      response->set_expires_at(
          std::chrono::duration_cast<std::chrono::seconds>(
              expiry.time_since_epoch())
              .count());

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status GetUserPermissions(
      ServerContext *context,
      const multitenant::v1::GetUserPermissionsRequest *request,
      multitenant::v1::GetUserPermissionsResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("GetUserPermissions: user={} (tenant={})", request->user_id(),
                 tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      // Ensure permissions table exists
      conn->execute(R"(
        CREATE TABLE IF NOT EXISTS permissions (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          user_id INTEGER NOT NULL,
          resource TEXT NOT NULL,
          action TEXT NOT NULL,
          allowed INTEGER DEFAULT 1,
          created_at TEXT,
          UNIQUE(user_id, resource, action)
        )
      )");

      auto stmt = conn->prepare(
          "SELECT id, user_id, resource, action, allowed FROM permissions "
          "WHERE user_id = ?");
      stmt.bind(1, request->user_id());

      while (stmt.step()) {
        auto *perm = response->add_permissions();
        perm->set_id(stmt.column<int64_t>(0));
        perm->set_user_id(stmt.column<int64_t>(1));
        perm->set_resource(stmt.column<std::string>(2));
        perm->set_action(stmt.column<std::string>(3));
        perm->set_allowed(stmt.column<int64_t>(4) != 0);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status GrantPermission(
      ServerContext *context,
      const multitenant::v1::GrantPermissionRequest *request,
      multitenant::v1::GrantPermissionResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("GrantPermission: user={} resource={} action={} (tenant={})",
                 request->user_id(), request->resource(), request->action(),
                 tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      // Ensure permissions table exists
      conn->execute(R"(
        CREATE TABLE IF NOT EXISTS permissions (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          user_id INTEGER NOT NULL,
          resource TEXT NOT NULL,
          action TEXT NOT NULL,
          allowed INTEGER DEFAULT 1,
          created_at TEXT,
          UNIQUE(user_id, resource, action)
        )
      )");

      // Insert or update (upsert)
      auto stmt = conn->prepare(R"(
        INSERT INTO permissions (user_id, resource, action, allowed, created_at)
        VALUES (?, ?, ?, 1, datetime('now'))
        ON CONFLICT(user_id, resource, action)
        DO UPDATE SET allowed = 1
      )");
      stmt.bind(1, request->user_id());
      stmt.bind(2, request->resource());
      stmt.bind(3, request->action());
      stmt.step();

      // Fetch the permission
      auto fetch_stmt = conn->prepare(
          "SELECT id, user_id, resource, action, allowed FROM permissions "
          "WHERE user_id = ? AND resource = ? AND action = ?");
      fetch_stmt.bind(1, request->user_id());
      fetch_stmt.bind(2, request->resource());
      fetch_stmt.bind(3, request->action());

      if (fetch_stmt.step()) {
        auto *perm = response->mutable_permission();
        perm->set_id(fetch_stmt.column<int64_t>(0));
        perm->set_user_id(fetch_stmt.column<int64_t>(1));
        perm->set_resource(fetch_stmt.column<std::string>(2));
        perm->set_action(fetch_stmt.column<std::string>(3));
        perm->set_allowed(fetch_stmt.column<int64_t>(4) != 0);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status RevokePermission(
      ServerContext *context,
      const multitenant::v1::RevokePermissionRequest *request,
      multitenant::v1::RevokePermissionResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::info("RevokePermission: user={} resource={} action={} (tenant={})",
                 request->user_id(), request->resource(), request->action(),
                 tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      auto stmt = conn->prepare(
          "DELETE FROM permissions WHERE user_id = ? AND resource = ? "
          "AND action = ?");
      stmt.bind(1, request->user_id());
      stmt.bind(2, request->resource());
      stmt.bind(3, request->action());
      stmt.step();

      response->set_success(true);
      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

  Status
  CheckPermission(ServerContext *context,
                  const multitenant::v1::CheckPermissionRequest *request,
                  multitenant::v1::CheckPermissionResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    spdlog::debug("CheckPermission: user={} resource={} action={} (tenant={})",
                  request->user_id(), request->resource(), request->action(),
                  tenant_id);

    try {
      auto &pool = manager_.get_pool(tenant_id);
      auto conn = pool.acquire();

      // Ensure permissions table exists
      conn->execute(R"(
        CREATE TABLE IF NOT EXISTS permissions (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          user_id INTEGER NOT NULL,
          resource TEXT NOT NULL,
          action TEXT NOT NULL,
          allowed INTEGER DEFAULT 1,
          created_at TEXT,
          UNIQUE(user_id, resource, action)
        )
      )");

      auto stmt = conn->prepare(
          "SELECT allowed FROM permissions "
          "WHERE user_id = ? AND resource = ? AND action = ?");
      stmt.bind(1, request->user_id());
      stmt.bind(2, request->resource());
      stmt.bind(3, request->action());

      if (stmt.step()) {
        response->set_allowed(stmt.column<int64_t>(0) != 0);
      } else {
        // No explicit permission found - default to false
        response->set_allowed(false);
      }

      return Status::OK;
    } catch (const std::exception &e) {
      return Status(StatusCode::INTERNAL, e.what());
    }
  }

private:
  tenant::TenantManager &manager_;
};

/**
 * Run the gRPC server
 */
void run_server(const std::string &address, tenant::TenantManager &manager) {
  TenantServiceImpl tenant_service(manager);
  UserServiceImpl user_service(manager);

  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&tenant_service);
  builder.RegisterService(&user_service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  spdlog::info("Server listening on {}", address);

  server->Wait();
}

int main(int argc, char **argv) {
  // Setup logging
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  spdlog::info("╔════════════════════════════════════════════╗");
  spdlog::info("║  Step 06: gRPC Server                      ║");
  spdlog::info("╚════════════════════════════════════════════╝");

  try {
    // Initialize tenant manager
    tenant::TenantManager manager("system.db", "data/tenants/");

    // Run server
    std::string address = "0.0.0.0:50051";
    run_server(address, manager);

  } catch (const std::exception &e) {
    spdlog::error("Server error: {}", e.what());
    return 1;
  }

  return 0;
}
