#pragma once

#include <chrono>
#include <string>

namespace pool {

/**
 * Configuration for the connection pool
 */
struct PoolConfig {
  // Database settings
  std::string db_path;           // Path to SQLite database
  bool create_if_missing = true; // Create database if not exists

  // Pool sizing
  size_t min_connections = 1;  // Minimum connections to maintain
  size_t max_connections = 10; // Maximum connections allowed

  // Timeouts
  std::chrono::milliseconds acquire_timeout{5000}; // Max wait for connection
  std::chrono::milliseconds idle_timeout{60000}; // Close idle connections after
  std::chrono::milliseconds health_check_interval{
      30000}; // Health check frequency

  // SQLite pragmas
  bool enable_foreign_keys = true;
  bool enable_wal_mode = true;
  std::string synchronous = "NORMAL";
  int busy_timeout_ms = 5000;

  // Validation
  void validate() const {
    if (db_path.empty()) {
      throw std::invalid_argument("db_path cannot be empty");
    }
    if (min_connections > max_connections) {
      throw std::invalid_argument(
          "min_connections cannot exceed max_connections");
    }
    if (max_connections == 0) {
      throw std::invalid_argument("max_connections must be at least 1");
    }
  }
};

/**
 * Pool statistics for monitoring
 */
struct PoolStats {
  size_t total_connections = 0;     // Total connections ever created
  size_t active_connections = 0;    // Currently checked out
  size_t available_connections = 0; // Ready in pool
  size_t waiting_threads = 0;       // Threads waiting for connection
  size_t peak_connections = 0;      // Maximum concurrent connections

  // Lifetime counters
  size_t total_acquisitions = 0;   // Successful acquires
  size_t total_releases = 0;       // Returns to pool
  size_t timeouts = 0;             // Acquisition timeouts
  size_t failed_health_checks = 0; // Connections that failed health check

  // Timing (microseconds)
  double avg_acquire_time_us = 0; // Average time to acquire
  double max_acquire_time_us = 0; // Maximum acquire time
};

} // namespace pool
