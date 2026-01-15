# Step 06: gRPC Basics

This step introduces gRPC for building high-performance RPC services with full
CRUD operations and permission management.

## What's New in This Step

- Protocol Buffers (protobuf) definitions
- gRPC service definitions with 16 RPC methods
- Code generation with CMake
- Full server implementation with database integration
- Client demonstration
- vcpkg integration for gRPC

## Why gRPC?

gRPC provides:
- **High performance**: Binary protocol, HTTP/2
- **Strong typing**: Protocol Buffers schema
- **Code generation**: Client/server stubs in multiple languages
- **Streaming**: Bidirectional streaming support
- **Interceptors**: Middleware for cross-cutting concerns

## Key Concepts

### 1. Protocol Buffers

Define your data structures and services:

```protobuf
syntax = "proto3";

package multitenant.v1;

message User {
    int64 id = 1;
    string tenant_id = 2;
    string username = 3;
    string email = 4;
    string role = 5;
    bool active = 6;
}

service UserService {
    rpc GetUser(GetUserRequest) returns (GetUserResponse);
    rpc ListUsers(ListUsersRequest) returns (ListUsersResponse);
    rpc CreateUser(CreateUserRequest) returns (CreateUserResponse);
}
```

### 2. CMake Code Generation

```cmake
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# Generate C++ code from .proto files
add_library(proto_lib
    ${CMAKE_CURRENT_BINARY_DIR}/multitenant.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/multitenant.grpc.pb.cc
)

protobuf_generate(TARGET proto_lib)
protobuf_generate(TARGET proto_lib LANGUAGE grpc
                  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
                  PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
```

### 3. Server Implementation

```cpp
class UserServiceImpl final : public UserService::Service {
    grpc::Status GetUser(grpc::ServerContext* context,
                        const GetUserRequest* request,
                        GetUserResponse* response) override {
        // Set tenant context from metadata
        TenantScope scope(get_tenant_from_context(context));

        // Use repository
        auto user = user_repo_.find_by_id(request->user_id());
        if (user) {
            *response->mutable_user() = to_proto(*user);
            return grpc::Status::OK;
        }
        return grpc::Status(grpc::NOT_FOUND, "User not found");
    }
};
```

### 4. Client Usage

```cpp
auto channel = grpc::CreateChannel("localhost:50051",
                                   grpc::InsecureChannelCredentials());
auto stub = UserService::NewStub(channel);

grpc::ClientContext context;
context.AddMetadata("x-tenant-id", "acme-corp");
context.AddMetadata("authorization", "Bearer " + token);

GetUserRequest request;
request.set_user_id(123);

GetUserResponse response;
auto status = stub->GetUser(&context, request, &response);
```

## Implemented RPC Methods

### TenantService (6 methods)

| Method | Description |
|--------|-------------|
| `GetTenant` | Retrieve tenant by ID |
| `ListTenants` | List all active tenants with pagination |
| `CreateTenant` | Create and provision a new tenant |
| `UpdateTenant` | Update tenant name, plan, or active status |
| `DeleteTenant` | Soft or permanent tenant deletion |
| `ProvisionTenant` | Provision tenant database |

### UserService (10 methods)

| Method | Description |
|--------|-------------|
| `GetUser` | Retrieve user by ID |
| `GetUserByUsername` | Find user by username |
| `ListUsers` | List users with optional active filter |
| `CreateUser` | Create a new user |
| `UpdateUser` | Update user fields (username, email, password, role, active) |
| `DeleteUser` | Soft or permanent user deletion |
| `Authenticate` | Verify credentials and return token |
| `GetUserPermissions` | List all permissions for a user |
| `GrantPermission` | Grant a permission to a user |
| `RevokePermission` | Revoke a permission from a user |
| `CheckPermission` | Check if user has specific permission |

## Project Structure

```
step06-grpc-basics/
├── CMakeLists.txt
├── vcpkg.json
├── proto/
│   ├── tenant.proto          # Tenant service definition
│   └── user.proto            # User service definition
├── src/
│   ├── server.cpp            # Full gRPC server implementation
│   ├── client.cpp            # gRPC client demonstration
│   └── main.cpp              # Protobuf message demo
└── tests/
    └── grpc_test.cpp         # Unit tests (8 test cases)
```

## Proto Files

### tenant.proto
```protobuf
syntax = "proto3";
package multitenant.v1;

service TenantService {
    rpc GetTenant(GetTenantRequest) returns (GetTenantResponse);
    rpc ListTenants(ListTenantsRequest) returns (ListTenantsResponse);
    rpc CreateTenant(CreateTenantRequest) returns (CreateTenantResponse);
    rpc UpdateTenant(UpdateTenantRequest) returns (UpdateTenantResponse);
    rpc DeleteTenant(DeleteTenantRequest) returns (DeleteTenantResponse);
    rpc ProvisionTenant(ProvisionTenantRequest) returns (ProvisionTenantResponse);
}
```

### user.proto
```protobuf
syntax = "proto3";
package multitenant.v1;

service UserService {
    rpc GetUser(GetUserRequest) returns (GetUserResponse);
    rpc GetUserByUsername(GetUserByUsernameRequest) returns (GetUserResponse);
    rpc ListUsers(ListUsersRequest) returns (ListUsersResponse);
    rpc CreateUser(CreateUserRequest) returns (CreateUserResponse);
    rpc UpdateUser(UpdateUserRequest) returns (UpdateUserResponse);
    rpc DeleteUser(DeleteUserRequest) returns (DeleteUserResponse);
    rpc Authenticate(AuthenticateRequest) returns (AuthenticateResponse);
    rpc GetUserPermissions(GetUserPermissionsRequest) returns (GetUserPermissionsResponse);
    rpc GrantPermission(GrantPermissionRequest) returns (GrantPermissionResponse);
    rpc RevokePermission(RevokePermissionRequest) returns (RevokePermissionResponse);
    rpc CheckPermission(CheckPermissionRequest) returns (CheckPermissionResponse);
}
```

## Building

```bash
# vcpkg.json includes grpc and protobuf
cmake --preset=default
cmake --build build

# Run tests
./build/step06_test
# Output: All tests passed (25 assertions in 8 test cases)

# Run demo
./build/step06_demo

# Run server (in one terminal)
./build/step06_server

# Run client (in another terminal)
./build/step06_client
```

## Metadata for Multi-Tenancy

gRPC uses metadata (HTTP/2 headers) for context:

```cpp
// Server: Extract tenant from metadata
std::string get_tenant_id(grpc::ServerContext* context) {
    auto metadata = context->client_metadata();
    auto it = metadata.find("x-tenant-id");
    if (it != metadata.end()) {
        return std::string(it->second.data(), it->second.size());
    }
    throw std::runtime_error("Missing x-tenant-id header");
}

// Client: Add tenant to metadata
context.AddMetadata("x-tenant-id", tenant_id);
```

## Next Step

Step 07 implements the complete service layer with handlers.
