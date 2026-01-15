#pragma once

#include "db/database.hpp"
#include "services/dto.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace repository
{

    /**
     * Repository for user data persistence
     */
    class UserRepository
    {
    public:
        explicit UserRepository(std::shared_ptr<db::Database> database);

        /**
         * Initialize the users table schema
         */
        void initialize_schema();

        /**
         * Find user by ID
         */
        std::optional<services::UserModel> find_by_id(int64_t id);

        /**
         * Find user by email
         */
        std::optional<services::UserModel> find_by_email(const std::string& email);

        /**
         * Find user by username within a tenant
         */
        std::optional<services::UserModel> find_by_username(
            const std::string& tenant_id, const std::string& username);

        /**
         * Find all users in a tenant
         */
        std::vector<services::UserModel> find_by_tenant(const std::string& tenant_id);

        /**
         * Find all users
         */
        std::vector<services::UserModel> find_all();

        /**
         * Insert a new user
         * @return The ID of the inserted user
         */
        int64_t insert(const services::UserModel& user);

        /**
         * Update an existing user
         */
        void update(const services::UserModel& user);

        /**
         * Delete a user by ID
         */
        void remove(int64_t id);

        /**
         * Count users in a tenant
         */
        size_t count_by_tenant(const std::string& tenant_id);

        /**
         * Check if email exists
         */
        bool email_exists(const std::string& email);

        /**
         * Check if username exists in tenant
         */
        bool username_exists(const std::string& tenant_id, const std::string& username);

    private:
        std::shared_ptr<db::Database> database_;

        services::UserModel map_from_row(db::Statement& stmt);
    };

} // namespace repository
