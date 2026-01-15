# Step 10: Complete System

This final step integrates all components into a production-ready multi-tenant gRPC service with SQLite persistence, JWT authentication, health checks, and Docker deployment.

## Features

- SQLite database with RAII wrappers and schema management
- Repository pattern for data access with tenant isolation
- JWT-based authentication with interceptors
- Role-based access control (RBAC)
- gRPC health check service (grpc.health.v1)
- Docker deployment with multi-stage builds
- Comprehensive test suite

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              gRPC Clients                               │
└─────────────────────────────────┬───────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Interceptor Chain                               │
│  ┌───────────┐ → ┌───────────┐ → ┌───────────┐                          │
│  │  Logging  │   │   Auth    │   │  Tenant   │                          │
│  │Interceptor│   │Interceptor│   │Interceptor│                          │
│  └───────────┘   └─────┬─────┘   └───────────┘                          │
│                        │                                                │
│                        ▼                                                │
│                 ┌─────────────┐                                         │
│                 │JWT Validator│                                         │
│                 └─────────────┘                                         │
└─────────────────────────────────┬───────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                              Handlers                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                   │
│  │ UserHandler  │  │TenantHandler │  │HealthHandler │                   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                   │
└─────────┼─────────────────┼─────────────────┼───────────────────────────┘
          │                 │                 │
          ▼                 ▼                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                              Services                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                   │
│  │ UserService  │  │TenantService │  │HealthService │                   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                   │
└─────────┼─────────────────┼─────────────────┼───────────────────────────┘
          │                 │                 │
          ▼                 ▼                 │
┌─────────────────────────────────────────────┼───────────────────────────┐
│                           Repositories      │                           │
│  ┌────────────────┐  ┌────────────────┐     │                           │
│  │ UserRepository │  │TenantRepository│     │                           │
│  └───────┬────────┘  └───────┬────────┘     │                           │
└──────────┼───────────────────┼──────────────┼───────────────────────────┘
           │                   │              │
           └───────────────────┴──────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         SQLite Database                                 │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐             │
│  │     users      │  │    tenants     │  │     roles      │             │
│  ├────────────────┤  ├────────────────┤  ├────────────────┤             │
│  │ role_permissions│  │  user_roles   │  │ schema_version │             │
│  └────────────────┘  └────────────────┘  └────────────────┘             │
└─────────────────────────────────────────────────────────────────────────┘
```

## Project Structure

```
step10-complete-system/
├── CMakeLists.txt              # Build configuration
├── vcpkg.json                  # Dependencies
├── Dockerfile                  # Multi-stage Docker build
├── docker-compose.yml          # Container orchestration
├── config/
│   ├── config.json             # Development configuration
│   └── config.production.json  # Production configuration
├── proto/
│   ├── common.proto            # Shared message types
│   ├── tenant.proto            # Tenant service definition
│   ├── user.proto              # User service definition
│   └── health.proto            # Health check service
├── include/
│   ├── auth/
│   │   ├── authorization_service.hpp
│   │   ├── jwt_validator.hpp
│   │   ├── policy_engine.hpp
│   │   └── role_repository.hpp
│   ├── config/
│   │   └── server_config.hpp
│   ├── db/
│   │   ├── database.hpp
│   │   ├── exceptions.hpp
│   │   ├── schema_initializer.hpp
│   │   ├── statement.hpp
│   │   └── transaction.hpp
│   ├── handlers/
│   │   ├── health_handler.hpp
│   │   ├── tenant_handler.hpp
│   │   └── user_handler.hpp
│   ├── interceptors/
│   │   ├── auth_interceptor.hpp
│   │   ├── base_interceptor.hpp
│   │   ├── interceptor_factory.hpp
│   │   ├── logging_interceptor.hpp
│   │   └── tenant_interceptor.hpp
│   ├── repository/
│   │   ├── tenant_repository.hpp
│   │   └── user_repository.hpp
│   └── services/
│       ├── auth_service.hpp
│       ├── dto.hpp
│       ├── exceptions.hpp
│       ├── health_service.hpp
│       ├── mapper.hpp
│       ├── tenant_service.hpp
│       └── user_service.hpp
├── src/
│   ├── auth/
│   ├── config/
│   ├── db/
│   ├── handlers/
│   ├── interceptors/
│   ├── repository/
│   ├── services/
│   └── main.cpp
├── tests/
│   ├── grpc_interceptor_test.cpp
│   └── repository_test.cpp
└── scripts/
    └── init-db.sh
```

## Prerequisites

- C++20 compatible compiler
- CMake 3.21+
- vcpkg package manager

## Dependencies

- gRPC and Protocol Buffers
- SQLite3
- spdlog (logging)
- fmt (formatting)
- nlohmann-json (configuration)
- jwt-cpp (JWT handling)
- Catch2 (testing)

## Building

```bash
# Set vcpkg root (if not already set)
export VCPKG_ROOT="$HOME/vcpkg"

# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build -j4
```

## Running Tests

### Run All Tests

```bash
# Using ctest
ctest --test-dir build --output-on-failure

# Or run individually:
./build/grpc_complete_system_test
./build/repository_test
```

### Test Suites

| Test Suite | Description | Assertions |
|------------|-------------|------------|
| `grpc_complete_system_test` | gRPC interceptor chain tests | 19 assertions in 11 test cases |
| `repository_test` | Repository CRUD operations | 31 assertions in 2 test cases |

### Repository Tests Coverage

- **TenantRepository**: Insert, find, update, find all, exists check, activate/deactivate
- **UserRepository**: Insert, find by ID/email/username, update, find by tenant, exists checks

## Running the Server

### Local Development

```bash
# Run with default configuration (in-memory database)
./build/step10_server

# Run with configuration file
./build/step10_server --config=config/config.json

# Run with environment variables
JWT_SECRET="your-secret" ./build/step10_server
```

### Server Output

```
╔════════════════════════════════════════════╗
║  Step 10: Complete System                  ║
║  Environment: development                  ║
╚════════════════════════════════════════════╝
Configuration loaded:
  Server: 0.0.0.0:50053
  Log Level: info
  Database: sqlite (data/multitenant.db)
  Interceptors: Logging=true, Auth=true, Tenant=true
Initializing database...
Database connection established
Database schema initialized
Repositories created
JWT validator initialized
Services created with database backing
Interceptors registered with JWT validator
Health check service registered
Server listening on 0.0.0.0:50053
```

## Docker Deployment

### Build and Run

```bash
# Build the image
docker-compose build

# Run the container
docker-compose up -d

# Check status
docker-compose ps

# View logs
docker-compose logs -f multitenant-server

# Stop
docker-compose down
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `CONFIG_FILE` | Path to configuration file | `/app/config/config.json` |
| `DATABASE_PATH` | SQLite database path | `/app/data/multitenant.db` |
| `JWT_SECRET` | Secret for JWT signing | `development-secret-change-in-production` |
| `LOG_LEVEL` | Logging level | `info` |

## Configuration

### Development (config/config.json)

```json
{
  "environment": "development",
  "server": {
    "host": "0.0.0.0",
    "port": 50053
  },
  "database": {
    "type": "sqlite",
    "connection_string": "data/multitenant.db"
  },
  "interceptors": {
    "enable_logging": true,
    "enable_auth": true,
    "enable_tenant": true
  }
}
```

### Production (config/config.production.json)

```json
{
  "environment": "production",
  "server": {
    "host": "0.0.0.0",
    "port": 50053,
    "max_receive_message_size": 4194304
  },
  "logging": {
    "level": "info",
    "format": "json"
  },
  "database": {
    "connection_string": "/app/data/multitenant.db",
    "pool_size": 10
  }
}
```

## Health Check

The server implements the standard gRPC health check protocol (`grpc.health.v1.Health`).

```bash
# Using grpcurl
grpcurl -plaintext localhost:50053 grpc.health.v1.Health/Check

# Expected response
{
  "status": "SERVING"
}
```

## API Overview

### User Service (multitenant.v1.UserService)

| Method | Description |
|--------|-------------|
| `GetUser` | Get user by ID |
| `GetUserByUsername` | Get user by username (tenant-scoped) |
| `ListUsers` | List all users in tenant |
| `CreateUser` | Create a new user |
| `UpdateUser` | Update existing user |
| `DeleteUser` | Soft delete (deactivate) user |

### Tenant Service (multitenant.v1.TenantService)

| Method | Description |
|--------|-------------|
| `GetTenant` | Get tenant by ID |
| `ListTenants` | List all tenants |
| `CreateTenant` | Create a new tenant |
| `UpdateTenant` | Update existing tenant |
| `DeleteTenant` | Soft delete (deactivate) tenant |

## Database Schema

### Tables

- **schema_version**: Schema migration tracking
- **tenants**: Multi-tenant organization data
- **users**: User accounts (tenant-scoped)
- **roles**: RBAC role definitions
- **role_permissions**: Permission assignments per role
- **user_roles**: User-to-role assignments

### Default Data

On first startup, the server seeds:
- Demo tenant (`tenant_id: 'demo'`)
- Admin role with full CRUD permissions
- User role with read-only permissions

## Tutorial Completion

Congratulations! You've completed the multi-tenant gRPC tutorial with:

- ✅ SQLite database with RAII wrappers
- ✅ Schema management and migrations
- ✅ Repository pattern for data access
- ✅ Tenant isolation in all operations
- ✅ gRPC services with Protocol Buffers
- ✅ Interceptor chain (Logging → Auth → Tenant)
- ✅ JWT-based authentication
- ✅ Role-based access control (RBAC)
- ✅ Health check service
- ✅ Docker deployment
- ✅ Comprehensive test suite
