#pragma once

#include "db/database.hpp"

#include <memory>

namespace db
{

    /**
     * Schema initializer for database tables
     */
    class SchemaInitializer
    {
    public:
        explicit SchemaInitializer(std::shared_ptr<Database> database);

        /**
         * Initialize all tables
         */
        void initialize_all();

        /**
         * Initialize users table
         */
        void initialize_users_table();

        /**
         * Initialize tenants table
         */
        void initialize_tenants_table();

        /**
         * Initialize RBAC tables (roles, permissions, user_roles)
         */
        void initialize_roles_tables();

        /**
         * Seed default data (demo tenant, admin user, default roles)
         */
        void seed_default_data();

        static constexpr int SCHEMA_VERSION = 1;

    private:
        std::shared_ptr<Database> database_;

        void create_schema_version_table();
        int get_current_version();
        void set_version(int version);
    };

} // namespace db
