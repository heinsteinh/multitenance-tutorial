#include "handlers/health_handler.hpp"
#include "services/health_service.hpp"

#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include <thread>

#include "health.grpc.pb.h"

namespace handlers
{

    class HealthHandler : public grpc::health::v1::Health::Service
    {
    public:
        explicit HealthHandler(services::HealthService& service)
            : service_(service)
        {
        }

        grpc::Status Check(grpc::ServerContext* /*context*/,
                           const grpc::health::v1::HealthCheckRequest* request,
                           grpc::health::v1::HealthCheckResponse* response) override
        {
            spdlog::debug("Health check request for service: '{}'", request->service());

            if(service_.is_healthy())
            {
                response->set_status(
                    grpc::health::v1::HealthCheckResponse::SERVING);
            }
            else
            {
                response->set_status(
                    grpc::health::v1::HealthCheckResponse::NOT_SERVING);
            }

            return grpc::Status::OK;
        }

        grpc::Status Watch(grpc::ServerContext* context,
                           const grpc::health::v1::HealthCheckRequest* request,
                           grpc::ServerWriter<grpc::health::v1::HealthCheckResponse>*
                               writer) override
        {
            spdlog::debug("Health watch request for service: '{}'", request->service());

            // Send initial status
            grpc::health::v1::HealthCheckResponse response;
            if(service_.is_healthy())
            {
                response.set_status(
                    grpc::health::v1::HealthCheckResponse::SERVING);
            }
            else
            {
                response.set_status(
                    grpc::health::v1::HealthCheckResponse::NOT_SERVING);
            }
            writer->Write(response);

            // Keep connection open until cancelled
            while(!context->IsCancelled())
            {
                std::this_thread::sleep_for(std::chrono::seconds(5));

                if(service_.is_healthy())
                {
                    response.set_status(
                        grpc::health::v1::HealthCheckResponse::SERVING);
                }
                else
                {
                    response.set_status(
                        grpc::health::v1::HealthCheckResponse::NOT_SERVING);
                }
                writer->Write(response);
            }

            return grpc::Status::OK;
        }

    private:
        services::HealthService& service_;
    };

} // namespace handlers

std::unique_ptr<grpc::Service>
handlers::make_health_handler(services::HealthService& service)
{
    return std::make_unique<handlers::HealthHandler>(service);
}
