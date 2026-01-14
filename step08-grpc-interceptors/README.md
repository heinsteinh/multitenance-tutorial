# Step 08: gRPC Interceptors

This step implements interceptors for cross-cutting concerns like authentication, logging, and metrics.

## What's New in This Step

- Server-side interceptors
- Client-side interceptors
- Authentication interceptor
- Tenant context interceptor
- Logging and metrics interceptor
- Interceptor chaining

## Interceptor Architecture

```
                    Incoming Request
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   Interceptor Chain                         │
│  ┌─────────────────┐                                        │
│  │ LoggingInter-   │ → Log request, start timer             │
│  │ ceptor          │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ AuthInter-      │ → Validate token, extract user         │
│  │ ceptor          │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ TenantInter-    │ → Extract tenant, set context          │
│  │ ceptor          │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ MetricsInter-   │ → Track request metrics                │
│  │ ceptor          │                                        │
│  └────────┬────────┘                                        │
└───────────┼─────────────────────────────────────────────────┘
            │
            ▼
     Service Handler
```

## Key Concepts

### 1. Server Interceptor Interface

```cpp
class ServerInterceptor : public grpc::experimental::Interceptor {
public:
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(
                grpc::experimental::InterceptionHookPoints::
                    POST_RECV_INITIAL_METADATA)) {
            // Process incoming metadata
            auto* metadata = methods->GetRecvInitialMetadata();
            // ...
        }
        
        methods->Proceed();
    }
};
```

### 2. Authentication Interceptor

```cpp
class AuthInterceptor : public ServerInterceptor {
public:
    void Intercept(InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(POST_RECV_INITIAL_METADATA)) {
            auto* metadata = methods->GetRecvInitialMetadata();
            
            // Extract and validate token
            auto auth_header = get_metadata(metadata, "authorization");
            if (!auth_header.empty()) {
                auto token = parse_bearer_token(auth_header);
                auto claims = validate_jwt(token);
                
                // Store claims in context
                set_user_context(claims.user_id, claims.tenant_id);
            } else if (is_protected_method(methods->GetMethodName())) {
                // Reject unauthenticated request
                methods->FailHijackedRecvMessage();
                return;
            }
        }
        methods->Proceed();
    }
};
```

### 3. Tenant Context Interceptor

```cpp
class TenantInterceptor : public ServerInterceptor {
public:
    TenantInterceptor(TenantManager& manager) : manager_(manager) {}
    
    void Intercept(InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(POST_RECV_INITIAL_METADATA)) {
            auto* metadata = methods->GetRecvInitialMetadata();
            
            auto tenant_id = get_metadata(metadata, "x-tenant-id");
            if (!tenant_id.empty()) {
                // Validate tenant exists and is active
                if (!manager_.is_tenant_active(tenant_id)) {
                    methods->FailHijackedRecvMessage();
                    return;
                }
                
                // Set tenant context for this request
                TenantContext::set(tenant_id);
            }
        }
        methods->Proceed();
    }
};
```

### 4. Logging Interceptor

```cpp
class LoggingInterceptor : public ServerInterceptor {
public:
    void Intercept(InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(PRE_SEND_STATUS)) {
            auto status = methods->GetSendStatus();
            auto duration = get_request_duration();
            
            spdlog::info("{} {} {} {}ms",
                methods->GetMethodName(),
                TenantContext::try_get_tenant_id().value_or("-"),
                status.ok() ? "OK" : "ERROR",
                duration.count());
        }
        methods->Proceed();
    }
};
```

### 5. Interceptor Factory

```cpp
class InterceptorFactory 
    : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
    grpc::experimental::Interceptor* CreateServerInterceptor(
            grpc::experimental::ServerRpcInfo* info) override {
        return new ChainedInterceptor({
            std::make_unique<LoggingInterceptor>(),
            std::make_unique<AuthInterceptor>(jwt_validator_),
            std::make_unique<TenantInterceptor>(tenant_manager_),
            std::make_unique<MetricsInterceptor>(metrics_)
        });
    }
};
```

## Project Structure

```
step08-grpc-interceptors/
├── include/
│   └── interceptors/
│       ├── auth_interceptor.hpp
│       ├── tenant_interceptor.hpp
│       ├── logging_interceptor.hpp
│       ├── metrics_interceptor.hpp
│       └── interceptor_factory.hpp
├── src/
│   ├── interceptors/
│   │   ├── auth_interceptor.cpp
│   │   └── tenant_interceptor.cpp
│   └── main.cpp
```

## Server Setup with Interceptors

```cpp
ServerBuilder builder;

// Create interceptor factory
std::vector<std::unique_ptr<ServerInterceptorFactoryInterface>> factories;
factories.push_back(std::make_unique<InterceptorFactory>(
    tenant_manager, jwt_validator, metrics));

builder.experimental().SetInterceptorCreators(std::move(factories));
builder.AddListeningPort(address, credentials);
builder.RegisterService(&service);

auto server = builder.BuildAndStart();
```

## Client-Side Interceptors

```cpp
class ClientLoggingInterceptor : public grpc::experimental::Interceptor {
public:
    void Intercept(InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(PRE_SEND_INITIAL_METADATA)) {
            auto* metadata = methods->GetSendInitialMetadata();
            // Add tracing headers
            (*metadata)["x-request-id"] = generate_request_id();
        }
        methods->Proceed();
    }
};
```

## Next Step

Step 09 implements role-based access control (RBAC) and authorization.
