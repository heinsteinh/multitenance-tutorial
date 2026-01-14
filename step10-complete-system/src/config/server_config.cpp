#include "config/server_config.hpp"
#include <fstream>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>

namespace config {

// JSON serialization helpers
void to_json(nlohmann::json &j, const ServerConfig &cfg) {
  j = nlohmann::json{{"host", cfg.host},
                     {"port", cfg.port},
                     {"enable_port_reuse", cfg.enable_port_reuse},
                     {"max_receive_message_size", cfg.max_receive_message_size},
                     {"max_send_message_size", cfg.max_send_message_size}};
}

void from_json(const nlohmann::json &j, ServerConfig &cfg) {
  j.at("host").get_to(cfg.host);
  j.at("port").get_to(cfg.port);
  if (j.contains("enable_port_reuse"))
    j.at("enable_port_reuse").get_to(cfg.enable_port_reuse);
  if (j.contains("max_receive_message_size"))
    j.at("max_receive_message_size").get_to(cfg.max_receive_message_size);
  if (j.contains("max_send_message_size"))
    j.at("max_send_message_size").get_to(cfg.max_send_message_size);
}

void to_json(nlohmann::json &j, const LoggingConfig &cfg) {
  j = nlohmann::json{{"level", cfg.level},
                     {"format", cfg.format},
                     {"enable_console", cfg.enable_console},
                     {"log_file_path", cfg.log_file_path},
                     {"max_file_size", cfg.max_file_size},
                     {"max_files", cfg.max_files}};
}

void from_json(const nlohmann::json &j, LoggingConfig &cfg) {
  if (j.contains("level"))
    j.at("level").get_to(cfg.level);
  if (j.contains("format"))
    j.at("format").get_to(cfg.format);
  if (j.contains("enable_console"))
    j.at("enable_console").get_to(cfg.enable_console);
  if (j.contains("log_file_path"))
    j.at("log_file_path").get_to(cfg.log_file_path);
  if (j.contains("max_file_size"))
    j.at("max_file_size").get_to(cfg.max_file_size);
  if (j.contains("max_files"))
    j.at("max_files").get_to(cfg.max_files);
}

void to_json(nlohmann::json &j, const InterceptorConfig &cfg) {
  j = nlohmann::json{{"enable_logging", cfg.enable_logging},
                     {"enable_auth", cfg.enable_auth},
                     {"enable_tenant", cfg.enable_tenant},
                     {"enable_metrics", cfg.enable_metrics}};
}

void from_json(const nlohmann::json &j, InterceptorConfig &cfg) {
  if (j.contains("enable_logging"))
    j.at("enable_logging").get_to(cfg.enable_logging);
  if (j.contains("enable_auth"))
    j.at("enable_auth").get_to(cfg.enable_auth);
  if (j.contains("enable_tenant"))
    j.at("enable_tenant").get_to(cfg.enable_tenant);
  if (j.contains("enable_metrics"))
    j.at("enable_metrics").get_to(cfg.enable_metrics);
}

void to_json(nlohmann::json &j, const DatabaseConfig &cfg) {
  j = nlohmann::json{{"type", cfg.type},
                     {"connection_string", cfg.connection_string},
                     {"pool_size", cfg.pool_size},
                     {"connection_timeout", cfg.connection_timeout}};
}

void from_json(const nlohmann::json &j, DatabaseConfig &cfg) {
  if (j.contains("type"))
    j.at("type").get_to(cfg.type);
  if (j.contains("connection_string"))
    j.at("connection_string").get_to(cfg.connection_string);
  if (j.contains("pool_size"))
    j.at("pool_size").get_to(cfg.pool_size);
  if (j.contains("connection_timeout"))
    j.at("connection_timeout").get_to(cfg.connection_timeout);
}

void to_json(nlohmann::json &j, const SecurityConfig &cfg) {
  j = nlohmann::json{{"enable_tls", cfg.enable_tls},
                     {"cert_file", cfg.cert_file},
                     {"key_file", cfg.key_file},
                     {"ca_file", cfg.ca_file},
                     {"require_client_auth", cfg.require_client_auth}};
}

void from_json(const nlohmann::json &j, SecurityConfig &cfg) {
  if (j.contains("enable_tls"))
    j.at("enable_tls").get_to(cfg.enable_tls);
  if (j.contains("cert_file"))
    j.at("cert_file").get_to(cfg.cert_file);
  if (j.contains("key_file"))
    j.at("key_file").get_to(cfg.key_file);
  if (j.contains("ca_file"))
    j.at("ca_file").get_to(cfg.ca_file);
  if (j.contains("require_client_auth"))
    j.at("require_client_auth").get_to(cfg.require_client_auth);
}

void to_json(nlohmann::json &j, const AppConfig &cfg) {
  j = nlohmann::json{
      {"environment", cfg.environment}, {"server", cfg.server},
      {"logging", cfg.logging},         {"interceptors", cfg.interceptors},
      {"database", cfg.database},       {"security", cfg.security}};
}

void from_json(const nlohmann::json &j, AppConfig &cfg) {
  if (j.contains("environment"))
    j.at("environment").get_to(cfg.environment);
  if (j.contains("server"))
    j.at("server").get_to(cfg.server);
  if (j.contains("logging"))
    j.at("logging").get_to(cfg.logging);
  if (j.contains("interceptors"))
    j.at("interceptors").get_to(cfg.interceptors);
  if (j.contains("database"))
    j.at("database").get_to(cfg.database);
  if (j.contains("security"))
    j.at("security").get_to(cfg.security);
}

// AppConfig implementation
AppConfig AppConfig::load_from_file(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open configuration file: " + filepath);
  }

  try {
    nlohmann::json j;
    file >> j;
    AppConfig config = j.get<AppConfig>();
    config.validate();
    return config;
  } catch (const nlohmann::json::exception &e) {
    throw std::runtime_error("Failed to parse configuration file: " +
                             std::string(e.what()));
  }
}

AppConfig AppConfig::load_from_string(const std::string &json_str) {
  try {
    nlohmann::json j = nlohmann::json::parse(json_str);
    AppConfig config = j.get<AppConfig>();
    config.validate();
    return config;
  } catch (const nlohmann::json::exception &e) {
    throw std::runtime_error("Failed to parse configuration string: " +
                             std::string(e.what()));
  }
}

AppConfig AppConfig::get_default() {
  AppConfig config;
  // Default values are already set in struct definitions
  return config;
}

void AppConfig::validate() const {
  // Validate server config
  if (server.port < 1024 || server.port > 65535) {
    throw std::invalid_argument("Server port must be between 1024 and 65535");
  }
  if (server.host.empty()) {
    throw std::invalid_argument("Server host cannot be empty");
  }

  // Validate logging config
  const std::vector<std::string> valid_levels = {
      "trace", "debug", "info", "warn", "error", "critical", "off"};
  bool valid_level = false;
  for (const auto &level : valid_levels) {
    if (logging.level == level) {
      valid_level = true;
      break;
    }
  }
  if (!valid_level) {
    throw std::invalid_argument("Invalid logging level: " + logging.level);
  }

  // Validate database config
  if (database.pool_size < 1) {
    throw std::invalid_argument("Database pool size must be at least 1");
  }
  if (database.connection_timeout < 0) {
    throw std::invalid_argument(
        "Database connection timeout cannot be negative");
  }

  // Validate security config
  if (security.enable_tls) {
    if (security.cert_file.empty() || security.key_file.empty()) {
      throw std::invalid_argument(
          "TLS enabled but cert_file or key_file is missing");
    }
  }
}

std::string AppConfig::to_json(int indent) const {
  nlohmann::json j = *this;
  return j.dump(indent);
}

std::string AppConfig::get_server_address() const {
  return server.host + ":" + std::to_string(server.port);
}

void AppConfig::apply_logging_config() const {
  // Set log level
  if (logging.level == "trace") {
    spdlog::set_level(spdlog::level::trace);
  } else if (logging.level == "debug") {
    spdlog::set_level(spdlog::level::debug);
  } else if (logging.level == "info") {
    spdlog::set_level(spdlog::level::info);
  } else if (logging.level == "warn") {
    spdlog::set_level(spdlog::level::warn);
  } else if (logging.level == "error") {
    spdlog::set_level(spdlog::level::err);
  } else if (logging.level == "critical") {
    spdlog::set_level(spdlog::level::critical);
  } else if (logging.level == "off") {
    spdlog::set_level(spdlog::level::off);
  }

  // Create sinks based on configuration
  std::vector<spdlog::sink_ptr> sinks;

  if (logging.enable_console) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);
  }

  if (!logging.log_file_path.empty()) {
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logging.log_file_path, logging.max_file_size, logging.max_files);
    sinks.push_back(file_sink);
  }

  // If sinks were configured, create a new logger
  if (!sinks.empty()) {
    auto logger = std::make_shared<spdlog::logger>("multi_logger",
                                                   sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
  }

  // Set pattern based on format
  if (logging.format == "json") {
    spdlog::set_pattern(
        R"({"time":"%Y-%m-%d %H:%M:%S.%e","level":"%l","msg":"%v"})");
  } else if (logging.format == "custom") {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
  }
  // "default" format uses spdlog's default pattern
}

} // namespace config
