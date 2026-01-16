#include "tenant.grpc.pb.h"
#include "user.grpc.pb.h"

#include <catch2/catch_test_macros.hpp>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <random>
#include <string>

// Helper to generate unique identifiers for tests
std::string GenerateUniqueId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    return std::to_string(dis(gen));
}

class GrpcTestHelper
{
private:
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<multitenant::v1::UserService::Stub> user_stub;
    std::unique_ptr<multitenant::v1::TenantService::Stub> tenant_stub;

public:
    GrpcTestHelper()
    {
        channel     = grpc::CreateChannel("localhost:50053",
                                          grpc::InsecureChannelCredentials());
        user_stub   = multitenant::v1::UserService::NewStub(channel);
        tenant_stub = multitenant::v1::TenantService::NewStub(channel);
    }

    std::unique_ptr<multitenant::v1::UserService::Stub>& GetUserStub()
    {
        return user_stub;
    }

    std::unique_ptr<multitenant::v1::TenantService::Stub>& GetTenantStub()
    {
        return tenant_stub;
    }
};

// ============================================================================
// Interceptor Tests - Authentication
// ============================================================================

TEST_CASE("Interceptor - Request without auth header proceeds",
          "[interceptor][auth]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    // No authorization header - interceptor logs but proceeds

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    // Interceptor validates but doesn't reject - proceeds to handler
    REQUIRE(status.ok());
}

TEST_CASE("Interceptor - Valid token allows access", "[interceptor][auth]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token-123");

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    // Should succeed with valid token
    REQUIRE(status.ok());
    REQUIRE(response.users().size() >= 0);
}

TEST_CASE("Interceptor - Authorization format doesn't cause rejection",
          "[interceptor][auth]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "InvalidFormat token123");

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    // Interceptor validates but doesn't reject - proceeds
    REQUIRE(status.ok());
}

// ============================================================================
// Interceptor Tests - Tenant Context
// ============================================================================

TEST_CASE("Interceptor - Valid tenant ID accepted", "[interceptor][tenant]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "demo");

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    REQUIRE(status.ok());
}

TEST_CASE("Interceptor - Invalid tenant doesn't cause rejection",
          "[interceptor][tenant]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "invalid-tenant-xyz");

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    // Interceptor validates but doesn't reject - proceeds
    REQUIRE(status.ok());
}

TEST_CASE("Interceptor - Request works without tenant ID",
          "[interceptor][tenant]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    // No tenant ID - should still work

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    REQUIRE(status.ok());
}

// ============================================================================
// Interceptor Tests - Request Metadata
// ============================================================================

TEST_CASE("Interceptor - Request with custom headers",
          "[interceptor][metadata]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");
    context.AddMetadata("x-request-id", "test-request-123");

    multitenant::v1::ListUsersRequest request;
    multitenant::v1::ListUsersResponse response;

    grpc::Status status =
        test.GetUserStub()->ListUsers(&context, request, &response);

    REQUIRE(status.ok());
}

// ============================================================================
// Negative Test Cases - Error Handling
// ============================================================================

TEST_CASE("E2E - Create user with invalid email format",
          "[interceptor][validation][negative]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");

    multitenant::v1::CreateUserRequest request;
    request.set_username("testuser");
    request.set_email("invalid-email"); // Invalid format
    request.set_password("secure123");
    request.set_role("user");

    multitenant::v1::CreateUserResponse response;
    grpc::Status status =
        test.GetUserStub()->CreateUser(&context, request, &response);

    REQUIRE(!status.ok());
    REQUIRE(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_CASE("E2E - Create user with duplicate username",
          "[interceptor][conflict][negative]")
{
    GrpcTestHelper test;
    std::string unique_id = GenerateUniqueId();
    std::string username  = "duplicate-user-" + unique_id;
    std::string email     = username + "@example.com";

    // First creation - should succeed
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "test-tenant");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(email);
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->CreateUser(&context, request, &response);
        REQUIRE(status.ok());
    }

    // Duplicate attempt - should fail
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "test-tenant");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username); // Same username
        request.set_email("different@example.com");
        request.set_password("pass456");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->CreateUser(&context, request, &response);

        REQUIRE(!status.ok());
        // Could be ALREADY_EXISTS or INVALID_ARGUMENT depending on implementation
        REQUIRE((status.error_code() == grpc::StatusCode::ALREADY_EXISTS || status.error_code() == grpc::StatusCode::INVALID_ARGUMENT));
    }
}

TEST_CASE("E2E - Get non-existent user", "[interceptor][notfound][negative]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");

    multitenant::v1::GetUserRequest request;
    request.set_user_id(99999999); // Non-existent ID

    multitenant::v1::GetUserResponse response;
    grpc::Status status =
        test.GetUserStub()->GetUser(&context, request, &response);

    REQUIRE(!status.ok());
    REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
}

TEST_CASE("E2E - Create user with empty username",
          "[interceptor][validation][negative]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");

    multitenant::v1::CreateUserRequest request;
    request.set_username(""); // Empty username
    request.set_email("test@example.com");
    request.set_password("secure123");
    request.set_role("user");

    multitenant::v1::CreateUserResponse response;
    grpc::Status status =
        test.GetUserStub()->CreateUser(&context, request, &response);

    REQUIRE(!status.ok());
    REQUIRE(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_CASE("E2E - Update non-existent user",
          "[interceptor][notfound][negative]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");

    multitenant::v1::UpdateUserRequest request;
    request.set_user_id(99999999);
    request.set_username("newname");

    multitenant::v1::UpdateUserResponse response;
    grpc::Status status =
        test.GetUserStub()->UpdateUser(&context, request, &response);

    REQUIRE(!status.ok());
    REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
}

TEST_CASE("E2E - Delete non-existent user",
          "[interceptor][notfound][negative]")
{
    GrpcTestHelper test;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");

    multitenant::v1::DeleteUserRequest request;
    request.set_user_id(99999999);

    multitenant::v1::DeleteUserResponse response;
    grpc::Status status =
        test.GetUserStub()->DeleteUser(&context, request, &response);

    REQUIRE(!status.ok());
    REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
}

// ============================================================================
// Tenant Isolation Tests - Multi-tenancy Boundary Enforcement
// ============================================================================

TEST_CASE("Tenant isolation - User created in one tenant not accessible from another",
          "[tenant][isolation][database]")
{
    GrpcTestHelper test;

    std::string unique_id = GenerateUniqueId();
    std::string username  = "isolated-user-" + unique_id;
    std::string email     = username + "@example.com";
    int64_t user_id;

    // Create user in tenant-a
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-a");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(email);
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->CreateUser(&context, request, &response);
        REQUIRE(status.ok());
        user_id = response.user().id();
    }

    // Try to access from tenant-b - should fail or return not found
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-b"); // Different tenant!

        multitenant::v1::GetUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::GetUserResponse response;
        grpc::Status status =
            test.GetUserStub()->GetUser(&context, request, &response);

        // Should fail - either PERMISSION_DENIED or NOT_FOUND
        REQUIRE(!status.ok());
        REQUIRE((status.error_code() == grpc::StatusCode::PERMISSION_DENIED || status.error_code() == grpc::StatusCode::NOT_FOUND));
    }
}

TEST_CASE("Tenant isolation - List users only shows users from same tenant",
          "[tenant][isolation][database]")
{
    GrpcTestHelper test;

    std::string unique_id     = GenerateUniqueId();
    std::string tenant_a_user = "tenant-a-user-" + unique_id;
    std::string tenant_b_user = "tenant-b-user-" + unique_id;

    // Create user in tenant-a
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-a");

        multitenant::v1::CreateUserRequest request;
        request.set_username(tenant_a_user);
        request.set_email(tenant_a_user + "@example.com");
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        REQUIRE(test.GetUserStub()->CreateUser(&context, request, &response).ok());
    }

    // Create user in tenant-b
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-b");

        multitenant::v1::CreateUserRequest request;
        request.set_username(tenant_b_user);
        request.set_email(tenant_b_user + "@example.com");
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        REQUIRE(test.GetUserStub()->CreateUser(&context, request, &response).ok());
    }

    // List users from tenant-a - should not see tenant-b user
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-a");

        multitenant::v1::ListUsersRequest request;
        multitenant::v1::ListUsersResponse response;
        grpc::Status status =
            test.GetUserStub()->ListUsers(&context, request, &response);

        REQUIRE(status.ok());

        // Check that tenant-b user is not in the list
        bool found_tenant_b_user = false;
        for(const auto& user : response.users())
        {
            if(user.username() == tenant_b_user)
            {
                found_tenant_b_user = true;
                break;
            }
        }
        REQUIRE(!found_tenant_b_user);
    }
}

TEST_CASE("Tenant isolation - Cannot update user from different tenant",
          "[tenant][isolation][database]")
{
    GrpcTestHelper test;

    std::string unique_id = GenerateUniqueId();
    std::string username  = "isolated-update-" + unique_id;
    int64_t user_id;

    // Create user in tenant-a
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-a");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(username + "@example.com");
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        REQUIRE(test.GetUserStub()->CreateUser(&context, request, &response).ok());
        user_id = response.user().id();
    }

    // Try to update from tenant-b
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-b");

        multitenant::v1::UpdateUserRequest request;
        request.set_user_id(user_id);
        request.set_username("hacked-name");

        multitenant::v1::UpdateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->UpdateUser(&context, request, &response);

        REQUIRE(!status.ok());
        REQUIRE((status.error_code() == grpc::StatusCode::PERMISSION_DENIED || status.error_code() == grpc::StatusCode::NOT_FOUND));
    }
}

TEST_CASE("Tenant isolation - Cannot delete user from different tenant",
          "[tenant][isolation][database]")
{
    GrpcTestHelper test;

    std::string unique_id = GenerateUniqueId();
    std::string username  = "isolated-delete-" + unique_id;
    int64_t user_id;

    // Create user in tenant-a
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-a");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(username + "@example.com");
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        REQUIRE(test.GetUserStub()->CreateUser(&context, request, &response).ok());
        user_id = response.user().id();
    }

    // Try to delete from tenant-b
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-b");

        multitenant::v1::DeleteUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::DeleteUserResponse response;
        grpc::Status status =
            test.GetUserStub()->DeleteUser(&context, request, &response);

        REQUIRE(!status.ok());
        REQUIRE((status.error_code() == grpc::StatusCode::PERMISSION_DENIED || status.error_code() == grpc::StatusCode::NOT_FOUND));
    }

    // Verify user still exists in tenant-a
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "tenant-a");

        multitenant::v1::GetUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::GetUserResponse response;
        grpc::Status status =
            test.GetUserStub()->GetUser(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.user().username() == username);
    }
}

// ============================================================================
// End-to-End Tests with Interceptors
// ============================================================================

TEST_CASE("E2E - Create user with auth and tenant context",
          "[interceptor][e2e]")
{
    GrpcTestHelper test;

    std::string unique_id = GenerateUniqueId();
    std::string username  = "testuser" + unique_id;
    std::string email     = "testuser" + unique_id + "@example.com";

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "test-tenant");

    multitenant::v1::CreateUserRequest request;
    request.set_username(username);
    request.set_email(email);
    request.set_password("secure123");
    request.set_role("user");

    multitenant::v1::CreateUserResponse response;
    grpc::Status status =
        test.GetUserStub()->CreateUser(&context, request, &response);

    REQUIRE(status.ok());
    REQUIRE(response.user().username() == username);
    REQUIRE(response.user().email() == email);
}

TEST_CASE("E2E - Get user with all interceptors", "[interceptor][e2e]")
{
    GrpcTestHelper test;

    // First create a user
    std::string unique_id = GenerateUniqueId();
    std::string username  = "getuser" + unique_id;
    std::string email     = "getuser" + unique_id + "@example.com";

    int64_t user_id;
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(email);
        request.set_password("pass123");

        multitenant::v1::CreateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->CreateUser(&context, request, &response);
        REQUIRE(status.ok());
        user_id = response.user().id();
    }

    // Now get the user with full interceptor chain
    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "demo");
    context.AddMetadata("x-request-id", "get-user-test");

    multitenant::v1::GetUserRequest request;
    request.set_user_id(user_id);

    multitenant::v1::GetUserResponse response;
    grpc::Status status =
        test.GetUserStub()->GetUser(&context, request, &response);

    REQUIRE(status.ok());
    REQUIRE(response.user().id() == user_id);
    REQUIRE(response.user().username() == username);
}

TEST_CASE("E2E - Create tenant with interceptors",
          "[interceptor][e2e][tenant]")
{
    GrpcTestHelper test;

    std::string unique_id = GenerateUniqueId();
    std::string tenant_id = "tenant-" + unique_id;

    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer valid-token");
    context.AddMetadata("x-tenant-id", "demo");

    multitenant::v1::CreateTenantRequest request;
    request.set_tenant_id(tenant_id);
    request.set_name("Test Tenant " + unique_id);
    request.set_plan("basic");

    multitenant::v1::CreateTenantResponse response;
    grpc::Status status =
        test.GetTenantStub()->CreateTenant(&context, request, &response);

    REQUIRE(status.ok());
    REQUIRE(response.tenant().tenant_id() == tenant_id);
}

TEST_CASE("E2E - Multiple requests with same tenant context",
          "[interceptor][e2e]")
{
    GrpcTestHelper test;

    std::string tenant_id = "test-multi-tenant";

    // Request 1: List users
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", tenant_id);

        multitenant::v1::ListUsersRequest request;
        multitenant::v1::ListUsersResponse response;

        grpc::Status status =
            test.GetUserStub()->ListUsers(&context, request, &response);
        REQUIRE(status.ok());
    }

    // Request 2: List tenants
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", tenant_id);

        multitenant::v1::ListTenantsRequest request;
        multitenant::v1::ListTenantsResponse response;

        grpc::Status status =
            test.GetTenantStub()->ListTenants(&context, request, &response);
        REQUIRE(status.ok());
    }
}

// ============================================================================
// Database CRUD Tests - Full Lifecycle
// ============================================================================

TEST_CASE("Database - Complete user CRUD lifecycle",
          "[database][crud][e2e]")
{
    GrpcTestHelper test;

    std::string unique_id     = GenerateUniqueId();
    std::string username      = "crud-user-" + unique_id;
    std::string email         = username + "@example.com";
    std::string updated_email = username + "-updated@example.com";
    int64_t user_id;

    // CREATE
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::CreateUserRequest request;
        request.set_username(username);
        request.set_email(email);
        request.set_password("secure123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->CreateUser(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.user().username() == username);
        REQUIRE(response.user().email() == email);
        user_id = response.user().id();
        REQUIRE(user_id > 0);
    }

    // READ
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::GetUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::GetUserResponse response;
        grpc::Status status =
            test.GetUserStub()->GetUser(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.user().id() == user_id);
        REQUIRE(response.user().username() == username);
        REQUIRE(response.user().email() == email);
    }

    // UPDATE
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::UpdateUserRequest request;
        request.set_user_id(user_id);
        request.set_email(updated_email);

        multitenant::v1::UpdateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->UpdateUser(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.user().id() == user_id);
        REQUIRE(response.user().email() == updated_email);
    }

    // Verify update
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::GetUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::GetUserResponse response;
        grpc::Status status =
            test.GetUserStub()->GetUser(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.user().email() == updated_email);
    }

    // DELETE
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::DeleteUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::DeleteUserResponse response;
        grpc::Status status =
            test.GetUserStub()->DeleteUser(&context, request, &response);

        REQUIRE(status.ok());
    }

    // Verify deletion
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::GetUserRequest request;
        request.set_user_id(user_id);

        multitenant::v1::GetUserResponse response;
        grpc::Status status =
            test.GetUserStub()->GetUser(&context, request, &response);

        REQUIRE(!status.ok());
        REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
    }
}

TEST_CASE("Database - Complete tenant CRUD lifecycle",
          "[database][crud][tenant][e2e]")
{
    GrpcTestHelper test;

    std::string unique_id    = GenerateUniqueId();
    std::string tenant_id    = "crud-tenant-" + unique_id;
    std::string tenant_name  = "CRUD Test Tenant " + unique_id;
    std::string updated_name = "Updated " + tenant_name;

    // CREATE
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::CreateTenantRequest request;
        request.set_tenant_id(tenant_id);
        request.set_name(tenant_name);
        request.set_plan("basic");

        multitenant::v1::CreateTenantResponse response;
        grpc::Status status =
            test.GetTenantStub()->CreateTenant(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.tenant().tenant_id() == tenant_id);
        REQUIRE(response.tenant().name() == tenant_name);
    }

    // READ
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::GetTenantRequest request;
        request.set_tenant_id(tenant_id);

        multitenant::v1::GetTenantResponse response;
        grpc::Status status =
            test.GetTenantStub()->GetTenant(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.tenant().tenant_id() == tenant_id);
        REQUIRE(response.tenant().name() == tenant_name);
    }

    // UPDATE
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::UpdateTenantRequest request;
        request.set_tenant_id(tenant_id);
        request.set_name(updated_name);

        multitenant::v1::UpdateTenantResponse response;
        grpc::Status status =
            test.GetTenantStub()->UpdateTenant(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.tenant().name() == updated_name);
    }

    // DELETE
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::DeleteTenantRequest request;
        request.set_tenant_id(tenant_id);

        multitenant::v1::DeleteTenantResponse response;
        grpc::Status status =
            test.GetTenantStub()->DeleteTenant(&context, request, &response);

        REQUIRE(status.ok());
    }

    // Verify deletion
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "demo");

        multitenant::v1::GetTenantRequest request;
        request.set_tenant_id(tenant_id);

        multitenant::v1::GetTenantResponse response;
        grpc::Status status =
            test.GetTenantStub()->GetTenant(&context, request, &response);

        REQUIRE(!status.ok());
        REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
    }
}

TEST_CASE("Database - List operations return persisted data",
          "[database][list][e2e]")
{
    GrpcTestHelper test;

    std::string unique_id = GenerateUniqueId();
    std::vector<int64_t> created_user_ids;

    // Create multiple users
    for(int i = 0; i < 3; i++)
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "list-test");

        multitenant::v1::CreateUserRequest request;
        request.set_username("list-user-" + unique_id + "-" + std::to_string(i));
        request.set_email("list-user-" + unique_id + "-" + std::to_string(i) + "@example.com");
        request.set_password("pass123");
        request.set_role("user");

        multitenant::v1::CreateUserResponse response;
        grpc::Status status =
            test.GetUserStub()->CreateUser(&context, request, &response);

        REQUIRE(status.ok());
        created_user_ids.push_back(response.user().id());
    }

    // List users and verify all created users are present
    {
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer valid-token");
        context.AddMetadata("x-tenant-id", "list-test");

        multitenant::v1::ListUsersRequest request;
        multitenant::v1::ListUsersResponse response;

        grpc::Status status =
            test.GetUserStub()->ListUsers(&context, request, &response);

        REQUIRE(status.ok());
        REQUIRE(response.users().size() >= 3);

        // Verify all created users are in the list
        for(int64_t user_id : created_user_ids)
        {
            bool found = false;
            for(const auto& user : response.users())
            {
                if(user.id() == user_id)
                {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }
}
