#include <catch2/catch_test_macros.hpp>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <random>
#include <sstream>
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
    channel = grpc::CreateChannel("localhost:50052",
                                  grpc::InsecureChannelCredentials());
    user_stub = multitenant::v1::UserService::NewStub(channel);
    tenant_stub = multitenant::v1::TenantService::NewStub(channel);
  }

  bool IsServerReady() {
    return channel->GetState(false) == GRPC_CHANNEL_READY ||
           channel->WaitForStateChange(GRPC_CHANNEL_IDLE,
                                       std::chrono::system_clock::now() +
                                           std::chrono::seconds(2));
  }

  std::unique_ptr<multitenant::v1::UserService::Stub> &GetUserStub() {
    return user_stub;
  }

  std::unique_ptr<multitenant::v1::TenantService::Stub> &GetTenantStub() {
    return tenant_stub;
  }
};

// ============================================================================
// UserService Tests
// ============================================================================

TEST_CASE("UserService - CreateUser creates new user", "[grpc][user]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string username = "testuser" + unique_id;
  std::string email = "testuser" + unique_id + "@example.com";

  grpc::ClientContext context;
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
  REQUIRE(response.user().role() == "user");
  REQUIRE(response.user().active() == true);
  REQUIRE(response.user().id() > 0);
}

TEST_CASE("UserService - CreateUser rejects duplicate email",
          "[grpc][user][validation]") {
  GrpcTestHelper test;

  // First create a user
  {
    grpc::ClientContext context;
    multitenant::v1::CreateUserRequest request;
    request.set_username("user1");
    request.set_email("duplicate@example.com");
    request.set_password("pass123");
    request.set_role("user");

    multitenant::v1::CreateUserResponse response;
    test.GetUserStub()->CreateUser(&context, request, &response);
  }

  // Try to create another with same email
  grpc::ClientContext context;
  multitenant::v1::CreateUserRequest request;
  request.set_username("user2");
  request.set_email("duplicate@example.com");
  request.set_password("pass456");
  request.set_role("user");

  multitenant::v1::CreateUserResponse response;
  grpc::Status status =
      test.GetUserStub()->CreateUser(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);
  REQUIRE(status.error_message().find("already in use") != std::string::npos);
}

TEST_CASE("UserService - GetUser retrieves existing user", "[grpc][user]") {
  GrpcTestHelper test;

  // Create a user first
  std::string unique_id = GenerateUniqueId();
  std::string username = "gettest" + unique_id;
  std::string email = "gettest" + unique_id + "@example.com";

  int64_t user_id;
  {
    grpc::ClientContext context;
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

  // Now get it
  grpc::ClientContext context;
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

TEST_CASE("UserService - GetUser returns NOT_FOUND for non-existent user",
          "[grpc][user][error]") {
  GrpcTestHelper test;

  grpc::ClientContext context;
  multitenant::v1::GetUserRequest request;
  request.set_user_id(999999);

  multitenant::v1::GetUserResponse response;
  grpc::Status status =
      test.GetUserStub()->GetUser(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
  REQUIRE(status.error_message().find("not found") != std::string::npos);
}

TEST_CASE("UserService - ListUsers returns all users", "[grpc][user]") {
  GrpcTestHelper test;

  // Create multiple users
  for (int i = 0; i < 3; i++) {
    grpc::ClientContext context;
    multitenant::v1::CreateUserRequest request;
    request.set_username("listuser" + std::to_string(i));
    request.set_email("listuser" + std::to_string(i) + "@example.com");
    request.set_password("pass123");
    request.set_role("user");

    multitenant::v1::CreateUserResponse response;
    test.GetUserStub()->CreateUser(&context, request, &response);
  }

  // List all users
  grpc::ClientContext context;
  multitenant::v1::ListUsersRequest request;

  multitenant::v1::ListUsersResponse response;
  grpc::Status status =
      test.GetUserStub()->ListUsers(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.users().size() >= 3);
}

TEST_CASE("UserService - GetUserByUsername retrieves user by username",
          "[grpc][user]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string username = "uniqueuser" + unique_id;
  std::string email = "uniqueuser" + unique_id + "@example.com";

  // Create a user
  {
    grpc::ClientContext context;
    multitenant::v1::CreateUserRequest request;
    request.set_username(username);
    request.set_email(email);
    request.set_password("pass123");
    request.set_role("admin");

    multitenant::v1::CreateUserResponse response;
    test.GetUserStub()->CreateUser(&context, request, &response);
  }

  // Get by username
  grpc::ClientContext context;
  multitenant::v1::GetUserByUsernameRequest request;
  request.set_username(username);

  multitenant::v1::GetUserResponse response;
  grpc::Status status =
      test.GetUserStub()->GetUserByUsername(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.user().username() == username);
  REQUIRE(response.user().email() == email);
  REQUIRE(response.user().role() == "admin");
}

TEST_CASE("UserService - UpdateUser updates user fields", "[grpc][user]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string username = "updatetest" + unique_id;
  std::string email = "updatetest" + unique_id + "@example.com";
  std::string new_email = "newemail" + unique_id + "@example.com";

  // Create a user first
  int64_t user_id;
  {
    grpc::ClientContext context;
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

  // Update the user
  grpc::ClientContext context;
  multitenant::v1::UpdateUserRequest request;
  request.set_user_id(user_id);
  request.set_role("admin");
  request.set_email(new_email);

  multitenant::v1::UpdateUserResponse response;
  grpc::Status status =
      test.GetUserStub()->UpdateUser(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.user().id() == user_id);
  REQUIRE(response.user().role() == "admin");
  REQUIRE(response.user().email() == new_email);
}

TEST_CASE("UserService - DeleteUser deactivates user", "[grpc][user]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string username = "deletetest" + unique_id;
  std::string email = "deletetest" + unique_id + "@example.com";

  // Create a user
  int64_t user_id;
  {
    grpc::ClientContext context;
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

  // Delete the user
  grpc::ClientContext context;
  multitenant::v1::DeleteUserRequest request;
  request.set_user_id(user_id);

  multitenant::v1::DeleteUserResponse response;
  grpc::Status status =
      test.GetUserStub()->DeleteUser(&context, request, &response);

  REQUIRE(status.ok());
}

// ============================================================================
// TenantService Tests
// ============================================================================

TEST_CASE("TenantService - CreateTenant creates new tenant", "[grpc][tenant]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string tenant_id = "test-tenant-" + unique_id;

  grpc::ClientContext context;
  multitenant::v1::CreateTenantRequest request;
  request.set_tenant_id(tenant_id);
  request.set_name("Test Tenant Inc");
  request.set_plan("pro");

  multitenant::v1::CreateTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->CreateTenant(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.tenant().tenant_id() == tenant_id);
  REQUIRE(response.tenant().name() == "Test Tenant Inc");
  REQUIRE(response.tenant().plan() == "pro");
  REQUIRE(response.tenant().active() == true);
  REQUIRE(response.tenant().id() > 0);
}

TEST_CASE("TenantService - CreateTenant rejects duplicate tenant_id",
          "[grpc][tenant][validation]") {
  GrpcTestHelper test;

  // Create first tenant
  {
    grpc::ClientContext context;
    multitenant::v1::CreateTenantRequest request;
    request.set_tenant_id("duplicate-tenant");
    request.set_name("First Tenant");
    request.set_plan("basic");

    multitenant::v1::CreateTenantResponse response;
    test.GetTenantStub()->CreateTenant(&context, request, &response);
  }

  // Try to create duplicate
  grpc::ClientContext context;
  multitenant::v1::CreateTenantRequest request;
  request.set_tenant_id("duplicate-tenant");
  request.set_name("Second Tenant");
  request.set_plan("enterprise");

  multitenant::v1::CreateTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->CreateTenant(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_CASE("TenantService - GetTenant retrieves existing tenant",
          "[grpc][tenant]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string tenant_id = "get-test-tenant-" + unique_id;

  // Create a tenant first
  {
    grpc::ClientContext context;
    multitenant::v1::CreateTenantRequest request;
    request.set_tenant_id(tenant_id);
    request.set_name("Get Test Tenant");
    request.set_plan("enterprise");

    multitenant::v1::CreateTenantResponse response;
    grpc::Status status =
        test.GetTenantStub()->CreateTenant(&context, request, &response);
    REQUIRE(status.ok());
  }

  // Get the tenant
  grpc::ClientContext context;
  multitenant::v1::GetTenantRequest request;
  request.set_tenant_id(tenant_id);

  multitenant::v1::GetTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->GetTenant(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.tenant().tenant_id() == tenant_id);
  REQUIRE(response.tenant().name() == "Get Test Tenant");
  REQUIRE(response.tenant().plan() == "enterprise");
}

TEST_CASE("TenantService - GetTenant returns NOT_FOUND for non-existent tenant",
          "[grpc][tenant][error]") {
  GrpcTestHelper test;

  grpc::ClientContext context;
  multitenant::v1::GetTenantRequest request;
  request.set_tenant_id("non-existent-tenant");

  multitenant::v1::GetTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->GetTenant(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
}

TEST_CASE("TenantService - ListTenants returns all tenants", "[grpc][tenant]") {
  GrpcTestHelper test;

  // Create multiple tenants
  for (int i = 0; i < 3; i++) {
    grpc::ClientContext context;
    multitenant::v1::CreateTenantRequest request;
    request.set_tenant_id("list-tenant-" + std::to_string(i));
    request.set_name("List Tenant " + std::to_string(i));
    request.set_plan("basic");

    multitenant::v1::CreateTenantResponse response;
    test.GetTenantStub()->CreateTenant(&context, request, &response);
  }

  // List all tenants
  grpc::ClientContext context;
  multitenant::v1::ListTenantsRequest request;

  multitenant::v1::ListTenantsResponse response;
  grpc::Status status =
      test.GetTenantStub()->ListTenants(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.tenants().size() >= 3);
}

TEST_CASE("TenantService - UpdateTenant updates tenant fields",
          "[grpc][tenant]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string tenant_id = "update-test-tenant-" + unique_id;

  // Create a tenant
  {
    grpc::ClientContext context;
    multitenant::v1::CreateTenantRequest request;
    request.set_tenant_id(tenant_id);
    request.set_name("Original Name");
    request.set_plan("basic");

    multitenant::v1::CreateTenantResponse response;
    grpc::Status status =
        test.GetTenantStub()->CreateTenant(&context, request, &response);
    REQUIRE(status.ok());
  }

  // Update the tenant
  grpc::ClientContext context;
  multitenant::v1::UpdateTenantRequest request;
  request.set_tenant_id(tenant_id);
  request.set_name("Updated Name");
  request.set_plan("enterprise");

  multitenant::v1::UpdateTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->UpdateTenant(&context, request, &response);

  REQUIRE(status.ok());
  REQUIRE(response.tenant().tenant_id() == tenant_id);
  REQUIRE(response.tenant().name() == "Updated Name");
  REQUIRE(response.tenant().plan() == "enterprise");
}

TEST_CASE("TenantService - DeleteTenant deactivates tenant", "[grpc][tenant]") {
  GrpcTestHelper test;

  std::string unique_id = GenerateUniqueId();
  std::string tenant_id = "delete-test-tenant-" + unique_id;

  // Create a tenant
  {
    grpc::ClientContext context;
    multitenant::v1::CreateTenantRequest request;
    request.set_tenant_id(tenant_id);
    request.set_name("Delete Test");
    request.set_plan("basic");

    multitenant::v1::CreateTenantResponse response;
    grpc::Status status =
        test.GetTenantStub()->CreateTenant(&context, request, &response);
    REQUIRE(status.ok());
  }

  // Delete the tenant
  grpc::ClientContext context;
  multitenant::v1::DeleteTenantRequest request;
  request.set_tenant_id(tenant_id);

  multitenant::v1::DeleteTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->DeleteTenant(&context, request, &response);

  REQUIRE(status.ok());
}

// ============================================================================
// Edge Cases and Validation Tests
// ============================================================================

TEST_CASE("UserService - CreateUser validates required fields",
          "[grpc][user][validation]") {
  GrpcTestHelper test;

  grpc::ClientContext context;
  multitenant::v1::CreateUserRequest request;
  // Missing username and email
  request.set_password("pass123");
  request.set_role("user");

  multitenant::v1::CreateUserResponse response;
  grpc::Status status =
      test.GetUserStub()->CreateUser(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_CASE("TenantService - CreateTenant validates required fields",
          "[grpc][tenant][validation]") {
  GrpcTestHelper test;

  grpc::ClientContext context;
  multitenant::v1::CreateTenantRequest request;
  // Missing tenant_id and name
  request.set_plan("basic");

  multitenant::v1::CreateTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->CreateTenant(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_CASE("UserService - UpdateUser validates user exists",
          "[grpc][user][validation]") {
  GrpcTestHelper test;

  grpc::ClientContext context;
  multitenant::v1::UpdateUserRequest request;
  request.set_user_id(999999);
  request.set_role("admin");

  multitenant::v1::UpdateUserResponse response;
  grpc::Status status =
      test.GetUserStub()->UpdateUser(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
}

TEST_CASE("TenantService - UpdateTenant validates tenant exists",
          "[grpc][tenant][validation]") {
  GrpcTestHelper test;

  grpc::ClientContext context;
  multitenant::v1::UpdateTenantRequest request;
  request.set_tenant_id("non-existent");
  request.set_name("New Name");

  multitenant::v1::UpdateTenantResponse response;
  grpc::Status status =
      test.GetTenantStub()->UpdateTenant(&context, request, &response);

  REQUIRE(!status.ok());
  REQUIRE(status.error_code() == grpc::StatusCode::NOT_FOUND);
}
