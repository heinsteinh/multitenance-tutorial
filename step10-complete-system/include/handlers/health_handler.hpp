#pragma once

#include "services/health_service.hpp"

#include <grpcpp/grpcpp.h>
#include <memory>

namespace handlers
{

    /**
     * Factory function to create health check handler
     * Returns a gRPC service that implements grpc.health.v1.Health
     */
    std::unique_ptr<grpc::Service> make_health_handler(services::HealthService& service);

} // namespace handlers
