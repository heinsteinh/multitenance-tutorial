#include "repository/user_repository.hpp"
#include "tenant/tenant_context.hpp"
#include "tenant/tenant_manager.hpp"

#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

void print_header() {
  spdlog::info("╔════════════════════════════════════════════╗");
  spdlog::info("║  Step 05: Tenant Management Demo          ║");
  spdlog::info("╚════════════════════════════════════════════╝");
  spdlog::info("");
}

void demo_tenant_provisioning(tenant::TenantManager &manager) {
  spdlog::info("=== Tenant Provisioning ===");

  // Create test tenants
  repository::Tenant acme{.id = 0,
                          .tenant_id = "acme-corp",
                          .name = "ACME Corporation",
                          .plan = "enterprise",
                          .active = true};

  repository::Tenant startup{.id = 0,
                             .tenant_id = "cool-startup",
                             .name = "Cool Startup",
                             .plan = "basic",
                             .active = true};

  manager.provision_tenant(acme);
  spdlog::info("Provisioned tenant: {}", acme.name);

  manager.provision_tenant(startup);
  spdlog::info("Provisioned tenant: {}", startup.name);

  spdlog::info("");
}

void demo_tenant_context() {
  spdlog::info("=== Tenant Context ===");

  // Set context for ACME
  tenant::TenantContext::set("acme-corp", 1);
  spdlog::info("Current tenant: {}", tenant::TenantContext::tenant_id());
  spdlog::info("Current user ID: {}", tenant::TenantContext::user_id());

  // Use RAII scope
  {
    tenant::TenantScope scope("cool-startup", 2);
    spdlog::info("Inside scope - tenant: {}",
                 tenant::TenantContext::tenant_id());
    spdlog::info("Inside scope - user: {}", tenant::TenantContext::user_id());
  }

  // Context restored
  spdlog::info("After scope - tenant: {}", tenant::TenantContext::tenant_id());
  spdlog::info("After scope - user: {}", tenant::TenantContext::user_id());

  tenant::TenantContext::clear();
  spdlog::info("");
}

void demo_tenant_isolation(tenant::TenantManager &manager) {
  spdlog::info("=== Tenant Data Isolation ===");

  // Create users in ACME tenant directly (bypass repository for simpler demo)
  {
    tenant::TenantScope scope("acme-corp", 1);
    auto &pool = manager.get_current_pool();
    auto conn = pool.acquire();

    conn->execute("INSERT INTO users (username, email, role) VALUES ('alice', "
                  "'alice@acme.com', 'admin')");
    conn->execute("INSERT INTO users (username, email, role) VALUES ('bob', "
                  "'bob@acme.com', 'user')");

    auto stmt = conn->prepare("SELECT COUNT(*) FROM users");
    stmt.step();
    int count = stmt.column<int>(0);
    spdlog::info("ACME tenant has {} users", count);

    auto list_stmt = conn->prepare("SELECT username, email FROM users");
    while (list_stmt.step()) {
      spdlog::info("  - {} <{}>", list_stmt.column<std::string>(0),
                   list_stmt.column<std::string>(1));
    }
  }

  // Create users in Startup tenant
  {
    tenant::TenantScope scope("cool-startup", 2);
    auto &pool = manager.get_current_pool();
    auto conn = pool.acquire();

    conn->execute("INSERT INTO users (username, email, role) VALUES "
                  "('charlie', 'charlie@startup.com', 'admin')");

    auto stmt = conn->prepare("SELECT COUNT(*) FROM users");
    stmt.step();
    int count = stmt.column<int>(0);
    spdlog::info("Startup tenant has {} users", count);

    auto list_stmt = conn->prepare("SELECT username, email FROM users");
    while (list_stmt.step()) {
      spdlog::info("  - {} <{}>", list_stmt.column<std::string>(0),
                   list_stmt.column<std::string>(1));
    }
  }

  spdlog::info("Data is isolated - each tenant has its own database");
  spdlog::info("");
}

void demo_pool_stats(tenant::TenantManager &manager) {
  spdlog::info("=== Connection Pool Stats ===");

  {
    tenant::TenantScope scope("acme-corp", 1);
    auto &pool = manager.get_current_pool();
    auto stats = pool.stats();

    spdlog::info("ACME pool:");
    spdlog::info("  Total created:    {}", stats.total_connections);
    spdlog::info("  Active:           {}", stats.active_connections);
    spdlog::info("  Available:        {}", stats.available_connections);
    spdlog::info("  Total acquires:   {}", stats.total_acquisitions);
  }

  {
    tenant::TenantScope scope("cool-startup", 2);
    auto &pool = manager.get_current_pool();
    auto stats = pool.stats();

    spdlog::info("Startup pool:");
    spdlog::info("  Total created:    {}", stats.total_connections);
    spdlog::info("  Active:           {}", stats.active_connections);
    spdlog::info("  Available:        {}", stats.available_connections);
    spdlog::info("  Total acquires:   {}", stats.total_acquisitions);
  }

  auto sys_stats = manager.get_system_pool().stats();
  spdlog::info("System pool:");
  spdlog::info("  Total created:    {}", sys_stats.total_connections);
  spdlog::info("  Active:           {}", sys_stats.active_connections);
  spdlog::info("  Available:        {}", sys_stats.available_connections);
  spdlog::info("  Total acquires:   {}", sys_stats.total_acquisitions);

  spdlog::info("");
}

int main() {
  try {
    print_header();

    // Setup directories
    fs::create_directories("data/tenants");

    // Create tenant manager
    tenant::TenantManagerConfig config{.system_db_path = "data/system.db",
                                       .tenant_db_directory = "data/tenants",
                                       .pool_min_connections = 1,
                                       .pool_max_connections = 3};

    spdlog::info("Creating tenant manager");
    spdlog::info("System DB: {}", config.system_db_path);
    spdlog::info("Tenant directory: {}", config.tenant_db_directory);
    spdlog::info("");

    tenant::TenantManager manager(config);

    // Run demos
    demo_tenant_provisioning(manager);
    demo_tenant_context();
    demo_tenant_isolation(manager);
    demo_pool_stats(manager);

    spdlog::info("=== Demo Complete ===");
    spdlog::info("Next: Step 06 - gRPC Basics");

    return 0;

  } catch (const std::exception &e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }
}
