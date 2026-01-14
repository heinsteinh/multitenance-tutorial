# Step 06: gRPC Basics

This step introduces gRPC for building high-performance RPC services.

## What's New in This Step

- Protocol Buffers (protobuf) definitions
- gRPC service definitions
- Code generation with CMake
- Basic client/server implementation
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

## Project Structure

```
step06-grpc-basics/
├── CMakeLists.txt
├── vcpkg.json
├── proto/
│   ├── common.proto          # Common types
│   ├── tenant.proto          # Tenant service
│   └── user.proto            # User service
├── include/
│   └── grpc/
│       └── proto_utils.hpp   # Proto conversion utilities
├── src/
│   ├── server.cpp            # gRPC server
│   ├── client.cpp            # gRPC client
│   └── main.cpp
└── tests/
    └── grpc_test.cpp
```

## Proto Files

### common.proto
```protobuf
syntax = "proto3";
package multitenant.v1;

message Timestamp {
    int64 seconds = 1;
    int32 nanos = 2;
}

message Pagination {
    int32 page = 1;
    int32 page_size = 2;
}

message Error {
    int32 code = 1;
    string message = 2;
}
```

### tenant.proto
```protobuf
syntax = "proto3";
package multitenant.v1;

service TenantService {
    rpc GetTenant(GetTenantRequest) returns (GetTenantResponse);
    rpc CreateTenant(CreateTenantRequest) returns (CreateTenantResponse);
    rpc ListTenants(ListTenantsRequest) returns (ListTenantsResponse);
}
```

## Building

```bash
# vcpkg.json includes grpc and protobuf
cmake --preset=default
cmake --build build

# Run server
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
