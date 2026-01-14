#pragma once

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace config {

/**
 * @brief Server configuration settings
 */
struct ServerConfig {
  std::string host = "0.0.0.0";
  int port = 50053;
  bool enable_port_reuse = true;
  int max_receive_message_size = -1; // -1 means unlimited
  int max_send_message_size = -1;    // -1 means unlimited
};

/**
 * @brief Logging configuration settings
 */
struct LoggingConfig {
  std::string level = "info"; // trace, debug, info, warn, error, critical, off
  std::string format = "default"; // default, json, custom
  bool enable_console = true;
  std::string log_file_path = "";
  size_t max_file_size = 10485760; // 10MB
  size_t max_files = 5;
};

/**
 * @brief Interceptor configuration settings
 */
struct InterceptorConfig {
  bool enable_logging = true;
  bool enable_auth = true;
  bool enable_tenant = true;
  bool enable_metrics = false;
};

/**
 * @brief Database configuration settings
 */
struct DatabaseConfig {
  std::string type = "sqlite";
  std::string connection_string = ":memory:";
  int pool_size = 10;
  int connection_timeout = 30; // seconds
};

/**
 * @brief Security configuration settings
 */
struct SecurityConfig {
  bool enable_tls = false;
  std::string cert_file = "";
  std::string key_file = "";
  std::string ca_file = "";
  bool require_client_auth = false;
};

/**
 * @brief Complete application configuration
 */
struct AppConfig {
  ServerConfig server;
  LoggingConfig logging;
  InterceptorConfig interceptors;
  DatabaseConfig database;
  SecurityConfig security;
  std::string environment = "development"; // development, staging, production

  /**
   * @brief Load configuration from JSON file
   * @param filepath Path to the configuration file
   * @return AppConfig loaded configuration
   * @throws std::runtime_error if file cannot be read or parsed
   */
  static AppConfig load_from_file(const std::string &filepath);

  /**
   * @brief Load configuration from JSON string
   * @param json_str JSON string containing configuration
   * @return AppConfig loaded configuration
   * @throws std::runtime_error if JSON cannot be parsed
   */
  static AppConfig load_from_string(const std::string &json_str);

  /**
   * @brief Get default configuration
   * @return AppConfig with default values
   */
  static AppConfig get_default();

  /**
   * @brief Validate configuration settings
   * @throws std::invalid_argument if configuration is invalid
   */
  void validate() const;

  /**
   * @brief Convert configuration to JSON string
   * @param indent Number of spaces for indentation (0 for compact)
   * @return std::string JSON representation
   */
  std::string to_json(int indent = 2) const;

  /**
   * @brief Get server address in format "host:port"
   * @return std::string server address
   */
  std::string get_server_address() const;

  /**
   * @brief Apply logging configuration to spdlog
   */
  void apply_logging_config() const;
};

} // namespace config
