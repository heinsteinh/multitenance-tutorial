#pragma once

#include "db/exceptions.hpp"
#include "db/statement.hpp"
#include "db/transaction.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>

namespace db {

/**
 * Configuration options for database connection
 */
struct DatabaseConfig {
  std::string path; // Database file path (":memory:" for in-memory)
  bool create_if_missing = true;      // Create file if it doesn't exist
  bool read_only = false;             // Open in read-only mode
  int busy_timeout_ms = 5000;         // Timeout for locked database
  bool enable_foreign_keys = true;    // Enable foreign key constraints
  bool enable_wal_mode = true;        // Use Write-Ahead Logging
  std::string synchronous = "NORMAL"; // Synchronous mode: OFF, NORMAL, FULL
};

/**
 * RAII wrapper for SQLite database connection.
 *
 * Usage:
 *   Database db(DatabaseConfig{.path = "app.db"});
 *   db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
 *
 *   auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
 *   stmt.bind(1, "Alice");
 *   stmt.step();
 */
class Database {
public:
  /**
   * Open or create a database
   * @param config Database configuration
   * @throws DatabaseException on failure
   */
  explicit Database(const DatabaseConfig &config);

  /**
   * Convenience constructor for simple cases
   * @param path Database file path
   */
  explicit Database(const std::string &path);

  ~Database();

  // Move-only
  Database(Database &&other) noexcept;
  Database &operator=(Database &&other) noexcept;
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  // ==================== Query Execution ====================

  /**
   * Execute a SQL statement that doesn't return results
   * @param sql SQL statement
   * @throws DatabaseException on failure
   */
  void execute(std::string_view sql);

  /**
   * Execute multiple SQL statements separated by semicolons
   * @param sql SQL statements
   * @throws DatabaseException on failure
   */
  void execute_script(std::string_view sql);

  /**
   * Prepare a SQL statement for execution
   * @param sql SQL statement with optional placeholders
   * @return Prepared statement
   */
  Statement prepare(std::string_view sql);

  /**
   * Execute a query and process results with a callback
   * @param sql SQL query
   * @param callback Function called for each row
   */
  template <typename Func> void query(std::string_view sql, Func &&callback) {
    auto stmt = prepare(sql);
    while (stmt.step()) {
      callback(stmt);
    }
  }

  /**
   * Execute a query and return first result
   * @param sql SQL query
   * @return Optional containing first row's first column, or nullopt if no rows
   */
  template <typename T> std::optional<T> query_single(std::string_view sql) {
    auto stmt = prepare(sql);
    if (stmt.step()) {
      return stmt.column<T>(0);
    }
    return std::nullopt;
  }

  // ==================== Transactions ====================

  /**
   * Begin a transaction
   * @param type Transaction type
   * @return Transaction object (commits/rolls back on destruction)
   */
  Transaction transaction(Transaction::Type type = Transaction::Type::Deferred);

  /**
   * Create a savepoint
   * @param name Savepoint name
   * @return Savepoint object
   */
  Savepoint savepoint(const std::string &name);

  // ==================== Utility ====================

  /**
   * Get the last inserted row ID
   */
  int64_t last_insert_rowid() const;

  /**
   * Get number of rows affected by last statement
   */
  int changes() const;

  /**
   * Get total rows affected since connection opened
   */
  int total_changes() const;

  /**
   * Check if database is in auto-commit mode (no active transaction)
   */
  bool is_autocommit() const;

  /**
   * Get the database file path
   */
  std::string path() const;

  /**
   * Get underlying SQLite handle
   */
  sqlite3 *handle() const { return db_; }

  /**
   * Check if table exists
   */
  bool table_exists(const std::string &table_name);

  /**
   * Get SQLite version string
   */
  static std::string sqlite_version();

private:
  sqlite3 *db_ = nullptr;
  DatabaseConfig config_;

  void apply_pragmas();
};

} // namespace db
