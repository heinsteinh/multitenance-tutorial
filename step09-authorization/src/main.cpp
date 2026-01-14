#include "auth/authorization_service.hpp"
#include "auth/jwt_validator.hpp"
#include "auth/role_repository.hpp"
#include "interceptors/interceptor_factory.hpp"
#include "services/auth_service.hpp"
#include "services/tenant_service.hpp"
#include "services/user_service.hpp"
#include "tenant.grpc.pb.h"
#include "user.grpc.pb.h"

#include <db/database.hpp>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

// Factory helpers
std::unique_ptr<multitenant::v1::UserService::Service>
make_user_handler(services::UserService& service);
std::unique_ptr<multitenant::v1::TenantService::Service>
make_tenant_handler(services::TenantService& service);

int main()
{
    spdlog::set_level(spdlog::level::info);
    spdlog::info("\n\n╔════════════════════════════════════════════╗\n║  Step "
                 "09: Authorization                    "
                 "║\n╚════════════════════════════════════════════╝");

    // ==================== Initialize Authorization Stack ====================

    // 1. Database layer - for role storage
    db::DatabaseConfig db_config{
        .path                = ":memory:", // Use in-memory database for demo
        .enable_foreign_keys = true,
        .enable_wal_mode     = false,
    };
    auto database = std::make_shared<db::Database>(db_config);

    // 2. Data access layer - role repository
    auto role_repository = std::make_shared<auth::RoleRepository>(database);

    // 3. JWT validation layer
    const std::string jwt_secret = "your-secret-key-min-32-chars-needed";
    auto jwt_validator           = auth::CreateJwtValidator(jwt_secret);

    // 4. Authorization logic layer
    auto authorization_service =
        auth::CreateAuthorizationService(role_repository);

    // 5. Service layer - high-level auth operations
    auto auth_service = std::make_shared<services::AuthService>(
        jwt_validator, authorization_service, role_repository);

    // ==================== Create Demo Roles ====================

    try
    {
        auto admin_role = role_repository->CreateRole("ADMIN");
        role_repository->AddPermission("ADMIN", "users", "create");
        role_repository->AddPermission("ADMIN", "users", "read");
        role_repository->AddPermission("ADMIN", "users", "update");
        role_repository->AddPermission("ADMIN", "users", "delete");

        auto editor_role = role_repository->CreateRole("EDITOR", "ADMIN");
        role_repository->AddPermission("EDITOR", "users", "read");
        role_repository->AddPermission("EDITOR", "users", "update");

        auto viewer_role = role_repository->CreateRole("VIEWER");
        role_repository->AddPermission("VIEWER", "users", "read");

        spdlog::info("✓ Demo roles created: ADMIN, EDITOR, VIEWER");
    }
    catch(const std::exception& e)
    {
        spdlog::error("Error creating demo roles: {}", e.what());
    }

    // ==================== Initialize gRPC Services ====================

    services::UserService user_service;
    services::TenantService tenant_service;

    auto user_handler   = make_user_handler(user_service);
    auto tenant_handler = make_tenant_handler(tenant_service);

    std::string server_address = "0.0.0.0:50053";
    grpc::ServerBuilder builder;

    // Enable SO_REUSEADDR for quick port reuse
    builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 1);

    // ==================== Register Interceptors ====================

    // Pass JWT validator to interceptor factory
    std::vector<
        std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
        interceptor_factories;
    interceptor_factories.push_back(
        std::make_unique<multitenant::InterceptorFactory>(jwt_validator));
    builder.experimental().SetInterceptorCreators(
        std::move(interceptor_factories));

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(user_handler.get());
    builder.RegisterService(tenant_handler.get());

    auto server = builder.BuildAndStart();
    if(!server)
    {
        spdlog::error("Failed to start gRPC server on {}", server_address);
        return 1;
    }

    spdlog::info("Server listening on {}", server_address);
    spdlog::info("Authorization stack initialized:");
    spdlog::info("  ✓ Interceptor layer (JWT validation)");
    spdlog::info("  ✓ Handler layer (authorization checks)");
    spdlog::info("  ✓ Service layer (business logic)");
    spdlog::info("  ✓ JWT validator configured");
    spdlog::info("  ✓ Role repository initialized");

    server->Wait();
    return 0;
}
