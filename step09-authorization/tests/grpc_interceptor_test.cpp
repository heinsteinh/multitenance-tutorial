#include <catch2/catch_test_macros.hpp>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <random>
#include <string>

#include "tenant.grpc.pb.h"
#include "user.grpc.pb.h"

// Helper to generate unique identifiers for tests
std::string GenerateUniqueId() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(100000, 999999);
  return std::to_string(dis(gen));
}

class GrpcTestHelper {
private:
  std::shared_ptr<grpc::Channel> channel;
  std::unique_ptr<multitenant::v1::UserService::Stub> user_stub;
  std::unique_ptr<multitenant::v1::TenantService::Stub> tenant_stub;

public:
  GrpcTestHelper() {
    channel = grpc::CreateChannel("localhost:50053",
                                  grpc::InsecureChannelCredentials());
    user_stub = multitenant::v1::UserService::NewStub(channel);
    tenant_stub = multitenant::v1::TenantService::NewStub(channel);
  }

  std::unique_ptr<multitenant::v1::UserService::Stub> &GetUserStub() {
    return user_stub;
  }

  std::unique_ptr<multitenant::v1::TenantService::Stub> &GetTenantStub() {
    return tenant_stub;
  }
};

// ============================================================================
// Interceptor Tests - Authentication
// ============================================================================

TEST_CASE("Interceptor - Request without auth header proceeds",
          "[interceptor][auth]") {
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

TEST_CASE("Interceptor - Valid token allows access", "[interceptor][auth]") {
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
          "[interceptor][auth]") {
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

TEST_CASE("Interceptor - Valid tenant ID accepted", "[interceptor][tenant]") {
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
          "[interceptor][tenant]") {
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
          "[interceptor][tenant]") {
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
          "[interceptor][metadata]") {
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
// End-to-End Tests with Interceptors
// ============================================================================

TEST_CASE("E2E - Create user with auth and tenant context",
          "[interceptor][e2e]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string username = "testuser" + unique_id;
  std::string email = "testuser" + unique_id + "@example.com";

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

TEST_CASE("E2E - Get user with all interceptors", "[interceptor][e2e]") {
  GrpcTestHelper test;

  // First create a user
  std::string unique_id = GenerateUniqueId();
  std::string username = "getuser" + unique_id;
  std::string email = "getuser" + unique_id + "@example.com";

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
          "[interceptor][e2e][tenant]") {
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
          "[interceptor][e2e]") {
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
