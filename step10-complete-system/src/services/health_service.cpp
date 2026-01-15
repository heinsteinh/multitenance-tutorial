#include "services/health_service.hpp"

#include <spdlog/spdlog.h>

namespace services
{

    HealthService::HealthService(std::shared_ptr<db::Database> database)
        : database_(std::move(database))
    {
    }

    bool HealthService::is_healthy() const
    {
        return is_database_healthy();
    }

    bool HealthService::is_database_healthy() const
    {
        try
        {
            // Simple query to verify database connectivity
            auto result = database_->query_single<int>("SELECT 1");
            return result.has_value() && result.value() == 1;
        }
        catch(const std::exception& ex)
        {
            spdlog::warn("Database health check failed: {}", ex.what());
            return false;
        }
    }

    std::string HealthService::get_status_message() const
    {
        if(is_healthy())
        {
            return "All systems operational";
        }

        std::string message = "Health check failed: ";
        if(!is_database_healthy())
        {
            message += "Database unavailable. ";
        }
        return message;
    }

} // namespace services
