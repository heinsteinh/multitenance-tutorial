# Step 07: gRPC Services

Building on Step 06's gRPC basics, this step implements a clean service layer architecture with handler classes, DTOs, validation, and error handling.

## What's New in This Step

- **Service layer abstraction** - Business logic separation from gRPC handlers
- **Handler classes** - gRPC service implementations that delegate to services
- **DTO mapping** - Clean proto ↔ domain type conversions
- **Input validation** - Email uniqueness, required fields, role validation
- **Error handling** - Exception hierarchy mapped to gRPC status codes
- **In-memory storage** - Demo data for testing without database
- **Fully functional gRPC services** - UserService and TenantService

## Quick Start

### Build
```bash
cd step07-grpc-services
cmake --build build -j4
```

### Run Server
```bash
./build/step07_server
# Output:
# ╔════════════════════════════════════════════╗
# ║  Step 07: gRPC Services                    ║
# ╚════════════════════════════════════════════╝
# [info] Server listening on 0.0.0.0:50052
```

### Test with grpcurl

**List UserService methods:**
```bash
grpcurl -plaintext -import-path ./proto -proto user.proto localhost:50052 list multitenant.v1.UserService
```

**Create a user:**
```bash
grpcurl -plaintext -import-path ./proto -proto user.proto \
  -d '{"username":"alice","email":"alice@example.com","password":"secret123","role":"user"}' \
  localhost:50052 multitenant.v1.UserService/CreateUser
```

Response:
```json
{
  "user": {
    "id": "2",
    "username": "alice",
    "email": "alice@example.com",
    "role": "user",
    "active": true
  }
}
```

**List all users:**
```bash
grpcurl -plaintext -import-path ./proto -proto user.proto \
  -d '{}' localhost:50052 multitenant.v1.UserService/ListUsers
```

**Get specific user:**
```bash
grpcurl -plaintext -import-path ./proto -proto user.proto \
  -d '{"user_id":1}' localhost:50052 multitenant.v1.UserService/GetUser
```

**Test validation (duplicate email):**
```bash
grpcurl -plaintext -import-path ./proto -proto user.proto \
  -d '{"username":"bob","email":"alice@example.com","password":"pass","role":"user"}' \
  localhost:50052 multitenant.v1.UserService/CreateUser
```

Response (error):
```
ERROR:
  Code: InvalidArgument
  Message: Email already in use
```

**Test not found error:**
```bash
grpcurl -plaintext -import-path ./proto -proto user.proto \
  -d '{"user_id":999}' localhost:50052 multitenant.v1.UserService/GetUser
```

Response (error):
```
ERROR:
  Code: NotFound
  Message: User not found
```

## Test Results ✅

### UserService Tests
| Method | Status | Details |
|--------|--------|---------|
| CreateUser | ✅ PASS | Successfully creates users with validation |
| ListUsers | ✅ PASS | Returns all users in database |
| GetUser | ✅ PASS | Retrieves individual users by ID |
| Validation | ✅ PASS | Duplicate email rejected with InvalidArgument |
| Error Handling | ✅ PASS | NotFound for non-existent users |

### TenantService Tests
| Method | Status | Details |
|--------|--------|---------|
| ListTenants | ✅ PASS | Returns all tenants |
| CreateTenant | ✅ PASS | Successfully creates new tenants |
| GetTenant | ✅ PASS | Retrieves tenants by ID |

### Test Coverage
- ✅ User creation with auto-incremented IDs
- ✅ Email uniqueness validation
- ✅ Tenant creation
- ✅ Exception mapping to gRPC status codes
- ✅ In-memory storage with demo data
- ✅ All RPC methods responding correctly

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        gRPC Layer                               │
│  ┌─────────────────┐ ┌─────────────────┐                       │
│  │ TenantHandler   │ │  UserHandler    │                       │
│  └────────┬────────┘ └────────┬────────┘                       │
│           │                   │                                │
│           ▼                   ▼                                │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    Service Layer                        │   │
│  │  ┌─────────────┐ ┌─────────────┐                       │   │
│  │  │TenantService│ │ UserService │                       │   │
│  │  └──────┬──────┘ └──────┬──────┘                       │   │
│  └─────────┼───────────────┼───────────────────────────────┘   │
│            │               │                                    │
│            ▼               ▼                                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │         In-Memory Storage (Demo Data)                   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

## Key Concepts

### 1. Handler Pattern

Handlers translate between gRPC and domain types:

```cpp
class UserHandler : public UserService::Service {
public:
    UserHandler(services::UserService& service)
        : service_(service) {}

    Status GetUser(ServerContext* context,
                   const GetUserRequest* request,
                   GetUserResponse* response) override {
        try {
            auto user = service_.get_user(request->user_id());
            *response->mutable_user() = mapper_.to_proto(user);
            return Status::OK;
        } catch (const NotFoundException& e) {
            return Status(NOT_FOUND, e.what());
        }
    }
};
```

### 2. Service Layer

Business logic lives in the service layer:

```cpp
class UserService {
public:
    User get_user(int64_t user_id) {
        auto user = users_.find(user_id);
        if (!user) {
            throw NotFoundException("User not found");
        }
        return *user;
    }

    User create_user(const CreateUserDto& dto) {
        // Validate
        if (dto.email.empty()) {
            throw ValidationException("Email required");
        }

        // Check uniqueness
        if (email_exists(dto.email)) {
            throw ValidationException("Email already in use");
        }

        // Create and store
        User user{
            .id = ++next_user_id_,
            .username = dto.username,
            .email = dto.email,
            .role = dto.role,
            .active = true
        };

        users_[user.id] = user;
        return user;
    }
};
```

### 3. DTO Mapping

Clean separation between proto and domain types:

```cpp
// Proto -> Domain
User from_proto(const multitenant::v1::User& proto) {
    return User{
        .id = proto.id(),
        .username = proto.username(),
        .email = proto.email(),
        .role = proto.role(),
        .active = proto.active()
    };
}

// Domain -> Proto
multitenant::v1::User to_proto(const User& user) {
    multitenant::v1::User proto;
    proto.set_id(user.id);
    proto.set_username(user.username);
    proto.set_email(user.email);
    proto.set_role(user.role);
    proto.set_active(user.active);
    return proto;
}
```

## Project Structure

```
step07-grpc-services/
├── include/
│   └── services/
│       ├── user_service.hpp      # User business logic
│       ├── tenant_service.hpp    # Tenant business logic
│       ├── dto.hpp               # Data transfer objects
│       ├── exceptions.hpp        # Service exceptions with gRPC mapping
│       └── mapper.hpp            # Proto ↔ Domain mapping
├── src/
│   ├── handlers/
│   │   ├── user_handler.cpp      # gRPC UserService implementation
│   │   └── tenant_handler.cpp    # gRPC TenantService implementation
│   ├── services/
│   │   ├── user_service.cpp      # User business logic implementation
│   │   ├── tenant_service.cpp    # Tenant business logic implementation
│   │   └── mapper.cpp            # Mapper implementation
│   └── main.cpp                  # gRPC server setup
├── proto/
│   ├── user.proto                # User service definition
│   ├── tenant.proto              # Tenant service definition
│   └── common.proto              # Shared message types
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

## Error Handling

Exception hierarchy mapped to gRPC status codes:

```cpp
class ServiceException : public std::runtime_error { };
class NotFoundException : public ServiceException { };
class ValidationException : public ServiceException { };
class AuthorizationException : public ServiceException { };

// Mapping
Status map_exception_to_status(const std::exception& e) {
    if (dynamic_cast<const NotFoundException*>(&e)) {
        return Status(NOT_FOUND, e.what());
    }
    if (dynamic_cast<const ValidationException*>(&e)) {
        return Status(INVALID_ARGUMENT, e.what());
    }
    if (dynamic_cast<const AuthorizationException*>(&e)) {
        return Status(PERMISSION_DENIED, e.what());
    }
    return Status(INTERNAL, e.what());
}
```

## Services Implemented

### UserService
- `GetUser` - Retrieve user by ID
- `GetUserByUsername` - Retrieve user by username
- `ListUsers` - List all users
- `CreateUser` - Create new user
- `UpdateUser` - Update existing user
- `DeleteUser` - Delete user
- `Authenticate` - Authenticate user (stub)
- `GetUserPermissions` - Get user permissions (stub)
- `GrantPermission` - Grant permission (stub)
- `RevokePermission` - Revoke permission (stub)
- `CheckPermission` - Check permission (stub)

### TenantService
- `GetTenant` - Retrieve tenant by ID
- `ListTenants` - List all tenants
- `CreateTenant` - Create new tenant
- `UpdateTenant` - Update tenant
- `DeleteTenant` - Delete tenant
- `ProvisionTenant` - Provision new tenant (stub)

## Build Details

**Build System:** CMake 3.21+
**Language:** C++20
**Dependencies:**
- gRPC 1.60+
- Protocol Buffers 4.25+
- spdlog 1.14+
- fmt 11.0+

**Build Outputs:**
- `libproto_lib.a` - Generated protobuf and gRPC code (1.0M)
- `libservice_lib.a` - Service implementations (33K)
- `libhandler_lib.a` - gRPC handlers (37K)
- `step07_server` - Executable server (19M)

## Next Steps

**Step 08: gRPC Interceptors**
- Add cross-cutting concerns (logging, authentication, tracing)
- Implement metadata handling
- Add request/response interceptors

**Step 09: Authorization**
- Implement tenant isolation
- Add role-based access control
- Integrate with Step 05 tenant context

**Step 10: Complete System**
- Integrate all components
- Add REST gateway
- Production configuration

## Testing Notes

The current implementation uses in-memory storage for demonstration purposes. Production use would integrate with:
- SQLite database (from Step 02)
- Connection pooling (from Step 03)
- Repository pattern (from Step 04)
- Tenant context management (from Step 05)

Demo data:
- 1 default admin user
- 1 default demo tenant
- Auto-incrementing user/tenant IDs
- Email uniqueness validation
- Role-based storage (admin, user roles)
