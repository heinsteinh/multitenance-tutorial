/**
 * Step 06: gRPC Client Implementation
 * 
 * Basic gRPC client demonstrating:
 * - Channel creation
 * - Stub usage
 * - Metadata (headers) for tenant context
 */

#include "tenant.grpc.pb.h"
#include "user.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

/**
 * Client for TenantService
 */
class TenantClient {
public:
    explicit TenantClient(std::shared_ptr<Channel> channel)
        : stub_(multitenant::v1::TenantService::NewStub(channel))
    {}

    bool CreateTenant(const std::string& tenant_id, 
                      const std::string& name,
                      const std::string& plan) {
        multitenant::v1::CreateTenantRequest request;
        request.set_tenant_id(tenant_id);
        request.set_name(name);
        request.set_plan(plan);

        multitenant::v1::CreateTenantResponse response;
        ClientContext context;

        Status status = stub_->CreateTenant(&context, request, &response);

        if (status.ok()) {
            spdlog::info("Created tenant: {} (ID={})", 
                         response.tenant().name(), 
                         response.tenant().id());
            return true;
        } else {
            spdlog::error("CreateTenant failed: {} - {}", 
                          static_cast<int>(status.error_code()),
                          status.error_message());
            return false;
        }
    }

    bool GetTenant(const std::string& tenant_id) {
        multitenant::v1::GetTenantRequest request;
        request.set_tenant_id(tenant_id);

        multitenant::v1::GetTenantResponse response;
        ClientContext context;

        Status status = stub_->GetTenant(&context, request, &response);

        if (status.ok()) {
            const auto& tenant = response.tenant();
            spdlog::info("Tenant: {} ({}) - Plan: {}, Active: {}",
                         tenant.name(),
                         tenant.tenant_id(),
                         tenant.plan(),
                         tenant.active() ? "yes" : "no");
            return true;
        } else {
            spdlog::error("GetTenant failed: {}", status.error_message());
            return false;
        }
    }

    bool ListTenants() {
        multitenant::v1::ListTenantsRequest request;
        request.set_active_only(true);

        multitenant::v1::ListTenantsResponse response;
        ClientContext context;

        Status status = stub_->ListTenants(&context, request, &response);

        if (status.ok()) {
            spdlog::info("Found {} tenants:", response.tenants_size());
            for (const auto& tenant : response.tenants()) {
                spdlog::info("  - {} ({}) [{}]",
                             tenant.name(),
                             tenant.tenant_id(),
                             tenant.plan());
            }
            return true;
        } else {
            spdlog::error("ListTenants failed: {}", status.error_message());
            return false;
        }
    }

private:
    std::unique_ptr<multitenant::v1::TenantService::Stub> stub_;
};

/**
 * Client for UserService (tenant-scoped)
 */
class UserClient {
public:
    UserClient(std::shared_ptr<Channel> channel, const std::string& tenant_id)
        : stub_(multitenant::v1::UserService::NewStub(channel))
        , tenant_id_(tenant_id)
    {}

    bool CreateUser(const std::string& username,
                    const std::string& email,
                    const std::string& role) {
        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(email);
        request.set_role(role);

        multitenant::v1::CreateUserResponse response;
        ClientContext context;
        
        // Add tenant context as metadata
        context.AddMetadata("x-tenant-id", tenant_id_);

        Status status = stub_->CreateUser(&context, request, &response);

        if (status.ok()) {
            spdlog::info("Created user: {} (ID={}) in tenant {}",
                         response.user().username(),
                         response.user().id(),
                         tenant_id_);
            return true;
        } else {
            spdlog::error("CreateUser failed: {}", status.error_message());
            return false;
        }
    }

    bool GetUser(int64_t user_id) {
        multitenant::v1::GetUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::GetUserResponse response;
        ClientContext context;
        context.AddMetadata("x-tenant-id", tenant_id_);

        Status status = stub_->GetUser(&context, request, &response);

        if (status.ok()) {
            const auto& user = response.user();
            spdlog::info("User: {} <{}> - Role: {}, Active: {}",
                         user.username(),
                         user.email(),
                         user.role(),
                         user.active() ? "yes" : "no");
            return true;
        } else {
            spdlog::error("GetUser failed: {}", status.error_message());
            return false;
        }
    }

    bool ListUsers() {
        multitenant::v1::ListUsersRequest request;
        request.set_active_only(true);

        multitenant::v1::ListUsersResponse response;
        ClientContext context;
        context.AddMetadata("x-tenant-id", tenant_id_);

        Status status = stub_->ListUsers(&context, request, &response);

        if (status.ok()) {
            spdlog::info("Found {} users in tenant {}:",
                         response.users_size(), tenant_id_);
            for (const auto& user : response.users()) {
                spdlog::info("  - {} <{}> [{}]",
                             user.username(),
                             user.email(),
                             user.role());
            }
            return true;
        } else {
            spdlog::error("ListUsers failed: {}", status.error_message());
            return false;
        }
    }

private:
    std::unique_ptr<multitenant::v1::UserService::Stub> stub_;
    std::string tenant_id_;
};

int main(int argc, char** argv) {
    // Setup logging
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("╔════════════════════════════════════════════╗");
    spdlog::info("║  Step 06: gRPC Client                      ║");
    spdlog::info("╚════════════════════════════════════════════╝");

    std::string target = "localhost:50051";
    spdlog::info("Connecting to {}", target);

    // Create channel
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());

    // Tenant operations
    TenantClient tenant_client(channel);
    
    spdlog::info("");
    spdlog::info("=== Tenant Operations ===");
    
    tenant_client.CreateTenant("demo-corp", "Demo Corporation", "pro");
    tenant_client.GetTenant("demo-corp");
    tenant_client.ListTenants();

    // User operations (scoped to tenant)
    UserClient user_client(channel, "demo-corp");
    
    spdlog::info("");
    spdlog::info("=== User Operations (tenant: demo-corp) ===");
    
    user_client.CreateUser("alice", "alice@demo.com", "admin");
    user_client.CreateUser("bob", "bob@demo.com", "user");
    user_client.ListUsers();
    user_client.GetUser(1);

    spdlog::info("");
    spdlog::info("=== Client Demo Complete ===");

    return 0;
}
