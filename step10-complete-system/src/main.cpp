#include "auth/jwt_validator.hpp"
#include "config/server_config.hpp"
#include "db/database.hpp"
#include "db/schema_initializer.hpp"
#include "handlers/health_handler.hpp"
#include "interceptors/interceptor_factory.hpp"
#include "repository/tenant_repository.hpp"
#include "repository/user_repository.hpp"
#include "services/health_service.hpp"
#include "services/tenant_service.hpp"
#include "services/user_service.hpp"
#include "health.grpc.pb.h"
#include "tenant.grpc.pb.h"
#include "user.grpc.pb.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

// Helper to find config file
std::string find_config_file(int argc, char* argv[])
{
    // Check command line argument
    for(int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg.find("--config=") == 0)
        {
            return arg.substr(9);
        }
    }

    // Check environment variable
    if(const char* env_config = std::getenv("CONFIG_FILE"))
    {
        return env_config;
    }

    // Check default locations
    std::vector<std::string> default_paths = {
        "config/config.json", "../config/config.json", "../../config/config.json",
        "/etc/multitenant/config.json"};

    for(const auto& path : default_paths)
    {
        if(std::filesystem::exists(path))
        {
            return path;
        }
    }

    // No config file found, will use defaults
    return "";
}

// Get JWT secret from environment or use default
std::string get_jwt_secret()
{
    if(const char* env_secret = std::getenv("JWT_SECRET"))
    {
        return env_secret;
    }
    // Default secret for development only
    return "development-secret-key-change-in-production";
}

int main(int argc, char* argv[])
{
    try
    {
        // Load configuration
        config::AppConfig app_config;
        std::string config_file = find_config_file(argc, argv);

        if(!config_file.empty())
        {
            spdlog::info("Loading configuration from: {}", config_file);
            app_config = config::AppConfig::load_from_file(config_file);
        }
        else
        {
            spdlog::warn("No configuration file found, using default settings");
            app_config = config::AppConfig::get_default();
        }

        // Apply logging configuration
        app_config.apply_logging_config();

        spdlog::info("\n\n╔════════════════════════════════════════════╗\n║  Step "
                     "10: Complete System             "
                     "║\n║  Environment: "
                     "{:<28}║\n╚════════════════════════════════════════════╝",
                     app_config.environment);

        spdlog::info("Configuration loaded:");
        spdlog::info("  Server: {}", app_config.get_server_address());
        spdlog::info("  Log Level: {}", app_config.logging.level);
        spdlog::info("  Database: {} ({})", app_config.database.type,
                     app_config.database.connection_string);
        spdlog::info("  Interceptors: Logging={}, Auth={}, Tenant={}",
                     app_config.interceptors.enable_logging,
                     app_config.interceptors.enable_auth,
                     app_config.interceptors.enable_tenant);

        // Initialize database
        spdlog::info("Initializing database...");
        db::DatabaseConfig db_config{
            .path               = app_config.database.connection_string,
            .busy_timeout_ms    = app_config.database.connection_timeout * 1000,
            .enable_foreign_keys = true,
            .enable_wal_mode    = true};

        auto database = std::make_shared<db::Database>(db_config);
        spdlog::info("Database connection established");

        // Initialize schema
        db::SchemaInitializer schema_init(database);
        schema_init.initialize_all();
        schema_init.seed_default_data();
        spdlog::info("Database schema initialized");

        // Create repositories
        auto user_repository   = std::make_shared<repository::UserRepository>(database);
        auto tenant_repository = std::make_shared<repository::TenantRepository>(database);
        spdlog::info("Repositories created");

        // Create JWT validator
        std::string jwt_secret = get_jwt_secret();
        auto jwt_validator     = auth::CreateJwtValidator(jwt_secret);
        spdlog::info("JWT validator initialized");

        // Create services with repository injection
        services::UserService user_service(user_repository);
        services::TenantService tenant_service(tenant_repository);
        services::HealthService health_service(database);
        spdlog::info("Services created with database backing");

        // Create handlers
        auto user_handler   = make_user_handler(user_service);
        auto tenant_handler = make_tenant_handler(tenant_service);
        auto health_handler = handlers::make_health_handler(health_service);

        grpc::ServerBuilder builder;

        // Configure server based on config
        if(app_config.server.enable_port_reuse)
        {
            builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 1);
        }

        if(app_config.server.max_receive_message_size > 0)
        {
            builder.SetMaxReceiveMessageSize(
                app_config.server.max_receive_message_size);
        }

        if(app_config.server.max_send_message_size > 0)
        {
            builder.SetMaxSendMessageSize(app_config.server.max_send_message_size);
        }

        // Register interceptors with JWT validator
        std::vector<
            std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
            interceptor_factories;
        interceptor_factories.push_back(
            std::make_unique<multitenant::InterceptorFactory>(jwt_validator));
        builder.experimental().SetInterceptorCreators(
            std::move(interceptor_factories));
        spdlog::info("Interceptors registered with JWT validator");

        // Configure security
        std::shared_ptr<grpc::ServerCredentials> credentials;
        if(app_config.security.enable_tls)
        {
            spdlog::info("TLS enabled, loading certificates...");
            grpc::SslServerCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = "";

            if(!app_config.security.cert_file.empty() &&
               !app_config.security.key_file.empty())
            {
                grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;

                // Read certificate file
                std::ifstream cert_file(app_config.security.cert_file);
                if(!cert_file)
                {
                    throw std::runtime_error("Failed to open certificate file: " +
                                             app_config.security.cert_file);
                }
                std::string cert_data((std::istreambuf_iterator<char>(cert_file)),
                                      std::istreambuf_iterator<char>());
                key_cert_pair.cert_chain = cert_data;

                // Read key file
                std::ifstream key_file(app_config.security.key_file);
                if(!key_file)
                {
                    throw std::runtime_error("Failed to open key file: " +
                                             app_config.security.key_file);
                }
                std::string key_data((std::istreambuf_iterator<char>(key_file)),
                                     std::istreambuf_iterator<char>());
                key_cert_pair.private_key = key_data;

                ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);

                // Read CA file if provided
                if(!app_config.security.ca_file.empty())
                {
                    std::ifstream ca_file(app_config.security.ca_file);
                    if(ca_file)
                    {
                        std::string ca_data((std::istreambuf_iterator<char>(ca_file)),
                                            std::istreambuf_iterator<char>());
                        ssl_opts.pem_root_certs = ca_data;
                    }
                }

                if(app_config.security.require_client_auth)
                {
                    ssl_opts.client_certificate_request =
                        GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;
                }

                credentials = grpc::SslServerCredentials(ssl_opts);
                spdlog::info("TLS configured with certificates");
            }
            else
            {
                throw std::runtime_error(
                    "TLS enabled but certificate or key file not specified");
            }
        }
        else
        {
            credentials = grpc::InsecureServerCredentials();
        }

        builder.AddListeningPort(app_config.get_server_address(), credentials);
        builder.RegisterService(user_handler.get());
        builder.RegisterService(tenant_handler.get());
        builder.RegisterService(health_handler.get());
        spdlog::info("Health check service registered");

        auto server = builder.BuildAndStart();
        if(!server)
        {
            spdlog::error("Failed to start gRPC server on {}",
                          app_config.get_server_address());
            return 1;
        }

        spdlog::info("Server listening on {}", app_config.get_server_address());
        spdlog::info("Press Ctrl+C to stop the server");

        server->Wait();
        return 0;
    }
    catch(const std::exception& ex)
    {
        spdlog::error("Server startup failed: {}", ex.what());
        return 1;
    }
}
