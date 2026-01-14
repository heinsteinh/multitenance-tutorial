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

  Status
  CheckPermission(ServerContext *context,
                  const multitenant::v1::CheckPermissionRequest *request,
                  multitenant::v1::CheckPermissionResponse *response) override {
    std::string tenant_id = get_tenant_id(context);
    if (tenant_id.empty()) {
      return Status(StatusCode::UNAUTHENTICATED, "Missing x-tenant-id header");
    }

    // Simplified permission check - in production use proper RBAC
    response->set_allowed(true);
    return Status::OK;
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
