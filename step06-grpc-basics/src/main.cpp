/**
 * Step 06: gRPC Basics Demo
 *
 * This demo shows the proto message usage without running a server.
 * For full server/client demo, run step06_server and step06_client separately.
 */

#include "tenant.pb.h"
#include "user.pb.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>

int main() {
  // Setup logging
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  spdlog::info("╔════════════════════════════════════════════╗");
  spdlog::info("║  Step 06: gRPC Basics Demo                 ║");
  spdlog::info("╚════════════════════════════════════════════╝");
  spdlog::info("");

  // Demonstrate protobuf message creation
  spdlog::info("=== Protobuf Message Demo ===");

  // Create a Tenant message
  multitenant::v1::Tenant tenant;
  tenant.set_id(1);
  tenant.set_tenant_id("acme-corp");
  tenant.set_name("ACME Corporation");
  tenant.set_plan("enterprise");
  tenant.set_active(true);

  spdlog::info("Created Tenant message:");
  spdlog::info("  ID: {}", tenant.id());
  spdlog::info("  Tenant ID: {}", tenant.tenant_id());
  spdlog::info("  Name: {}", tenant.name());
  spdlog::info("  Plan: {}", tenant.plan());
  spdlog::info("  Active: {}", tenant.active() ? "yes" : "no");

  // Create a User message
  multitenant::v1::User user;
  user.set_id(1);
  user.set_username("alice");
  user.set_email("alice@acme.com");
  user.set_role("admin");
  user.set_active(true);

  spdlog::info("");
  spdlog::info("Created User message:");
  spdlog::info("  ID: {}", user.id());
  spdlog::info("  Username: {}", user.username());
  spdlog::info("  Email: {}", user.email());
  spdlog::info("  Role: {}", user.role());

  // Serialize to binary
  std::string serialized;
  tenant.SerializeToString(&serialized);
  spdlog::info("");
  spdlog::info("Serialized tenant to {} bytes", serialized.size());

  // Deserialize
  multitenant::v1::Tenant parsed;
  parsed.ParseFromString(serialized);
  spdlog::info("Parsed tenant: {} ({})", parsed.name(), parsed.tenant_id());

  // Create request/response messages
  spdlog::info("");
  spdlog::info("=== Request/Response Messages ===");

  multitenant::v1::CreateUserRequest create_req;
  create_req.set_username("bob");
  create_req.set_email("bob@acme.com");
  create_req.set_password("secret123");
  create_req.set_role("user");

  spdlog::info("CreateUserRequest:");
  spdlog::info("  Username: {}", create_req.username());
  spdlog::info("  Email: {}", create_req.email());
  spdlog::info("  Role: {}", create_req.role());

  multitenant::v1::ListUsersResponse list_resp;
  auto *user1 = list_resp.add_users();
  user1->set_username("alice");
  user1->set_role("admin");

  auto *user2 = list_resp.add_users();
  user2->set_username("bob");
  user2->set_role("user");

  spdlog::info("");
  spdlog::info("ListUsersResponse with {} users:", list_resp.users_size());
  for (const auto &u : list_resp.users()) {
    spdlog::info("  - {} [{}]", u.username(), u.role());
  }

  spdlog::info("");
  spdlog::info("=== Demo Complete ===");
  spdlog::info("");
  spdlog::info("To run the full server/client demo:");
  spdlog::info("  1. ./step06_server  (in one terminal)");
  spdlog::info("  2. ./step06_client  (in another terminal)");
  spdlog::info("");
  spdlog::info("Next: Step 07 - gRPC Services (handlers and service layer)");

  return 0;
}
