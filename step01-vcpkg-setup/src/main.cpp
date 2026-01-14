/**
 * Step 01: vcpkg Setup Demo
 *
 * Demonstrates that vcpkg dependencies are properly integrated:
 * - spdlog for logging
 * - nlohmann/json for JSON handling
 * - fmt for string formatting
 */

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fmt/color.h>

#include <iostream>
#include <string>

using json = nlohmann::json;

int main() {
    // Configure spdlog
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

    spdlog::info("=== Step 01: vcpkg Setup Demo ===");
    spdlog::info("");

    // Demonstrate spdlog
    spdlog::debug("Debug message - spdlog is working");
    spdlog::info("Info message - with {} formatting", "fmt-style");
    spdlog::warn("Warning message");

    // Demonstrate nlohmann/json
    json config = {
        {"application", {
            {"name", "Multi-Tenant System"},
            {"version", "1.0.0"}
        }},
        {"database", {
            {"type", "sqlite"},
            {"pool_size", 10},
            {"timeout_ms", 5000}
        }},
        {"grpc", {
            {"port", 50051},
            {"max_threads", 4}
        }},
        {"tenants", {
            {"isolation", "database_per_tenant"},
            {"max_tenants", 100}
        }}
    };

    spdlog::info("");
    spdlog::info("Configuration (nlohmann/json):");
    std::cout << config.dump(2) << std::endl;

    // Demonstrate fmt
    spdlog::info("");
    spdlog::info("Formatted output (fmt):");

    std::string app_name = config["application"]["name"];
    int pool_size = config["database"]["pool_size"];

    fmt::print(fmt::emphasis::bold, "  Application: {}\n", app_name);
    fmt::print(fg(fmt::color::green), "  Pool Size: {}\n", pool_size);
    fmt::print(fg(fmt::color::cyan), "  gRPC Port: {}\n", config["grpc"]["port"].get<int>());

    // Demonstrate JSON access patterns
    spdlog::info("");
    spdlog::info("JSON access patterns:");

    // Safe access with contains()
    if (config.contains("tenants")) {
        spdlog::info("  Tenant isolation: {}",
            config["tenants"]["isolation"].get<std::string>());
    }

    // Value with default
    int max_connections = config.value("/database/max_connections"_json_pointer, 100);
    spdlog::info("  Max connections (default): {}", max_connections);

    // Iterate over object
    spdlog::info("  Top-level keys:");
    for (auto& [key, value] : config.items()) {
        spdlog::info("    - {}", key);
    }

    spdlog::info("");
    spdlog::info("=== vcpkg Integration Successful! ===");
    spdlog::info("");
    spdlog::info("Next: Step 02 - SQLite Foundation");

    return 0;
}
