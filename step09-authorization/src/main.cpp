#include "interceptors/interceptor_factory.hpp"
#include "services/tenant_service.hpp"
#include "services/user_service.hpp"

#include <grpcpp/grpcpp.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <vector>

#include "tenant.grpc.pb.h"
#include "user.grpc.pb.h"

// Factory helpers
std::unique_ptr<multitenant::v1::UserService::Service>
make_user_handler(services::UserService &service);
std::unique_ptr<multitenant::v1::TenantService::Service>
make_tenant_handler(services::TenantService &service);

int main() {
  spdlog::set_level(spdlog::level::info);
  spdlog::info("\n\n╔════════════════════════════════════════════╗\n║  Step "
               "09: Authorization                    "
               "║\n╚════════════════════════════════════════════╝");

  services::UserService user_service;
  services::TenantService tenant_service;

  auto user_handler = make_user_handler(user_service);
  auto tenant_handler = make_tenant_handler(tenant_service);

  std::string server_address = "0.0.0.0:50053";
  grpc::ServerBuilder builder;

  // Enable SO_REUSEADDR for quick port reuse
  builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 1);

  // Register interceptors
  std::vector<
      std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
      interceptor_factories;
  interceptor_factories.push_back(
      std::make_unique<multitenant::InterceptorFactory>());
  builder.experimental().SetInterceptorCreators(
      std::move(interceptor_factories));

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(user_handler.get());
  builder.RegisterService(tenant_handler.get());

  auto server = builder.BuildAndStart();
  if (!server) {
    spdlog::error("Failed to start gRPC server on {}", server_address);
    return 1;
  }

  spdlog::info("Server listening on {}", server_address);
  server->Wait();
  return 0;
}
