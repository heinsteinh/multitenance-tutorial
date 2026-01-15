#pragma once

#include "db/database.hpp"

#include <memory>
#include <string>

namespace services
{

    /**
     * Health check service for monitoring system health
     */
    class HealthService
    {
    public:
        explicit HealthService(std::shared_ptr<db::Database> database);

        /**
         * Check overall system health
         * @return true if all components are healthy
         */
        bool is_healthy() const;

        /**
         * Check database connectivity
         * @return true if database is accessible
         */
        bool is_database_healthy() const;

        /**
         * Get a human-readable status message
         */
        std::string get_status_message() const;

    private:
        std::shared_ptr<db::Database> database_;
    };

} // namespace services
