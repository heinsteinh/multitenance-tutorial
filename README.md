# C++ Multi-Tenant System with gRPC and SQLite

A progressive, step-by-step tutorial building a production-grade multi-tenant system in modern C++20. Each step builds directly on the previous, introducing one concept at a time.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         gRPC Clients                                │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      gRPC Interceptors                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │ AuthInterceptor │→ │TenantInterceptor│→ │ LoggingInterceptor  │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        gRPC Handlers                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │  TenantHandler  │  │  UserHandler    │  │  ResourceHandler    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         Services Layer                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │ TenantService   │  │  UserService    │  │  AuthService        │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       Repository Layer                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │TenantRepository │  │ UserRepository  │  │PermissionRepository │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Connection Pool                                │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │              SQLiteConnectionPool                           │    │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐    │    │
│  │  │ Conn 1 │ │ Conn 2 │ │ Conn 3 │ │ Conn N │ │  ...   │    │    │
│  │  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘    │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   SQLite Databases                                  │
│  ┌──────────────┐  ┌──────────────────────────────────────────┐     │
│  │   System DB  │  │           Tenant Databases               │     │
│  │  (tenants,   │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐    │     │
│  │   users,     │  │  │Tenant A │ │Tenant B │ │Tenant N │    │     │
│  │   perms)     │  │  └─────────┘ └─────────┘ └─────────┘    │     │
│  └──────────────┘  └──────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────┘
```

## Tutorial Steps

| Step | Topic | Key Concepts |
|------|-------|--------------|
| 01 | vcpkg Setup | Dependency management, CMake toolchain |
| 02 | SQLite Foundation | Database wrapper, RAII, prepared statements |
| 03 | Connection Pool | Thread-safe pooling, scoped connections |
| 04 | Repository Pattern | Generic CRUD, type-safe queries |
| 05 | Tenant Management | Multi-tenancy models, tenant isolation |
| 06 | gRPC Basics | Proto definitions, code generation |
| 07 | gRPC Services | Service implementation, handlers |
| 08 | gRPC Interceptors | Authentication, logging, metrics |
| 09 | Authorization | RBAC, permission checking |
| 10 | Complete System | Integration, testing, deployment |

## Multi-Tenancy Strategies

This tutorial implements **database-per-tenant** isolation:

```
Strategy Comparison:
─────────────────────────────────────────────────────────────
│ Strategy          │ Isolation │ Cost │ Complexity │ Scale │
├───────────────────┼───────────┼──────┼────────────┼───────┤
│ Shared Schema     │ Low       │ Low  │ Low        │ High  │
│ Schema-per-Tenant │ Medium    │ Med  │ Medium     │ Med   │
│ DB-per-Tenant     │ High      │ High │ High       │ Med   │
─────────────────────────────────────────────────────────────
```

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.21+
- vcpkg (automatically bootstrapped)
- Git

## Quick Start

```bash
# Clone and setup
git clone <this-repo>
cd cpp-multitenant-tutorial

# Start with Step 01
cd step01-vcpkg-setup
./bootstrap.sh  # or bootstrap.bat on Windows

# Build
cmake --preset=default
cmake --build build

# Run
./build/step01_vcpkg_demo
```

## Key Technologies

- **gRPC**: High-performance RPC framework
- **SQLite**: Embedded database engine
- **vcpkg**: C++ package manager
- **spdlog**: Fast logging library
- **nlohmann/json**: JSON for Modern C++
- **Catch2**: Testing framework

## Directory Structure

```
cpp-multitenant-tutorial/
├── README.md                    # This file
├── step01-vcpkg-setup/          # vcpkg and CMake foundation
├── step02-sqlite-foundation/    # SQLite wrapper and basics
├── step03-connection-pool/      # Thread-safe connection pooling
├── step04-repository-pattern/   # Generic repository implementation
├── step05-tenant-management/    # Multi-tenant database management
├── step06-grpc-basics/          # Protocol buffers and gRPC setup
├── step07-grpc-services/        # Service layer implementation
├── step08-grpc-interceptors/    # Request/response interceptors
├── step09-authorization/        # RBAC and permission system
└── step10-complete-system/      # Full integration
```

## Learning Path

Each step includes:
- `README.md` - Detailed explanation of concepts
- `CMakeLists.txt` - Build configuration
- `src/` - Implementation files
- `include/` - Header files
- `tests/` - Unit tests
- `main.cpp` - Demo application

Start with Step 01 and progress sequentially. Each step's code is self-contained but builds conceptually on previous steps.
