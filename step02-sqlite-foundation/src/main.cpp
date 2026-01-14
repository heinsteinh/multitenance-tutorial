/**
 * Step 02: SQLite Foundation Demo
 *
 * Demonstrates the database wrapper with:
 * - Connection management
 * - CRUD operations
 * - Prepared statements
 * - Transactions
 */

#include "db/database.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>

using json = nlohmann::json;

void setup_logging() {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
}

void demo_basic_operations(db::Database &database) {
  spdlog::info("=== Basic Operations ===");

  // Create table
  database.execute(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT UNIQUE,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");

  // Insert with prepared statement
  auto insert_stmt =
      database.prepare("INSERT INTO users (name, email) VALUES (?, ?)");

  insert_stmt.bind(1, "Alice");
  insert_stmt.bind(2, "alice@example.com");
  insert_stmt.step();

  int64_t alice_id = database.last_insert_rowid();
  spdlog::info("Inserted Alice with ID: {}", alice_id);

  // Reset and reuse statement
  insert_stmt.reset();
  insert_stmt.clear_bindings();
  insert_stmt.bind(1, "Bob");
  insert_stmt.bind(2, "bob@example.com");
  insert_stmt.step();

  spdlog::info("Inserted Bob with ID: {}", database.last_insert_rowid());

  // Query all users
  spdlog::info("All users:");
  database.query("SELECT id, name, email FROM users", [](db::Statement &stmt) {
    spdlog::info("  {} | {} | {}", stmt.column<int64_t>(0),
                 stmt.column<std::string>(1), stmt.column<std::string>(2));
  });
}

void demo_named_parameters(db::Database &database) {
  spdlog::info("");
  spdlog::info("=== Named Parameters ===");

  auto stmt = database.prepare(
      "SELECT * FROM users WHERE name = :name OR email = :email");

  stmt.bind(":name", "Alice");
  stmt.bind(":email", "nonexistent@example.com");

  while (stmt.step()) {
    spdlog::info("Found: {} ({})", stmt.column<std::string>(1),
                 stmt.column<std::string>(2));
  }
}

void demo_transactions(db::Database &database) {
  spdlog::info("");
  spdlog::info("=== Transactions ===");

  // Successful transaction
  {
    auto tx = database.transaction();

    database.execute("INSERT INTO users (name, email) VALUES ('Charlie', "
                     "'charlie@example.com')");
    database.execute("INSERT INTO users (name, email) VALUES ('Diana', "
                     "'diana@example.com')");

    tx.commit();
    spdlog::info("Transaction committed - Charlie and Diana added");
  }

  // Rolled back transaction
  {
    auto tx = database.transaction();

    database.execute(
        "INSERT INTO users (name, email) VALUES ('Eve', 'eve@example.com')");
    spdlog::info("Inserted Eve (will be rolled back)");

    // Transaction rolls back automatically when tx goes out of scope without
    // commit()
    spdlog::info("Transaction will rollback (no commit called)");
  }

  // Verify Eve was not added
  auto count = database.query_single<int>(
      "SELECT COUNT(*) FROM users WHERE name = 'Eve'");
  spdlog::info("Eve exists: {}", count.value_or(0) > 0 ? "yes" : "no");
}

void demo_savepoints(db::Database &database) {
  spdlog::info("");
  spdlog::info("=== Savepoints ===");

  auto tx = database.transaction();

  database.execute(
      "INSERT INTO users (name, email) VALUES ('Frank', 'frank@example.com')");

  {
    auto sp = database.savepoint("bulk_insert");

    try {
      database.execute("INSERT INTO users (name, email) VALUES ('Grace', "
                       "'grace@example.com')");
      // This will fail - duplicate email
      database.execute("INSERT INTO users (name, email) VALUES ('Grace2', "
                       "'grace@example.com')");
      sp.release();
    } catch (const db::ConstraintException &e) {
      spdlog::warn("Savepoint rolled back: {}", e.what());
      // Savepoint automatically rolls back in destructor
    }
  }

  // Frank should still be pending, Grace should be rolled back
  tx.commit();

  auto frank_exists = database.query_single<int>(
      "SELECT COUNT(*) FROM users WHERE name = 'Frank'");
  auto grace_exists = database.query_single<int>(
      "SELECT COUNT(*) FROM users WHERE name = 'Grace'");

  spdlog::info("Frank exists: {}", frank_exists.value_or(0) > 0 ? "yes" : "no");
  spdlog::info("Grace exists: {}", grace_exists.value_or(0) > 0 ? "yes" : "no");
}

void demo_optional_columns(db::Database &database) {
  spdlog::info("");
  spdlog::info("=== Optional/Nullable Columns ===");

  database.execute(R"(
        CREATE TABLE IF NOT EXISTS profiles (
            user_id INTEGER PRIMARY KEY,
            bio TEXT,
            website TEXT
        )
    )");

  // Insert with NULL values
  auto stmt = database.prepare(
      "INSERT INTO profiles (user_id, bio, website) VALUES (?, ?, ?)");
  stmt.bind(1, 1);
  stmt.bind(2, "Software developer");
  stmt.bind(3, nullptr); // NULL website
  stmt.step();

  // Query with optional
  auto query =
      database.prepare("SELECT bio, website FROM profiles WHERE user_id = ?");
  query.bind(1, 1);

  if (query.step()) {
    auto bio = query.column_optional<std::string>(0);
    auto website = query.column_optional<std::string>(1);

    spdlog::info("Bio: {}", bio.value_or("(none)"));
    spdlog::info("Website: {}", website.value_or("(none)"));
  }
}

void demo_utility_functions(db::Database &database) {
  spdlog::info("");
  spdlog::info("=== Utility Functions ===");

  spdlog::info("SQLite version: {}", db::Database::sqlite_version());
  spdlog::info("Database path: {}", database.path());
  spdlog::info("Is autocommit: {}", database.is_autocommit() ? "yes" : "no");
  spdlog::info("Table 'users' exists: {}",
               database.table_exists("users") ? "yes" : "no");
  spdlog::info("Table 'nonexistent' exists: {}",
               database.table_exists("nonexistent") ? "yes" : "no");

  auto user_count = database.query_single<int>("SELECT COUNT(*) FROM users");
  spdlog::info("Total users: {}", user_count.value_or(0));
}

int main() {
  setup_logging();

  spdlog::info("╔════════════════════════════════════════════╗");
  spdlog::info("║  Step 02: SQLite Foundation Demo           ║");
  spdlog::info("╚════════════════════════════════════════════╝");
  spdlog::info("");

  try {
    // Create in-memory database for demo
    db::Database database(db::DatabaseConfig{
        .path = ":memory:",
        .enable_foreign_keys = true,
        .enable_wal_mode = false // WAL not supported for in-memory
    });

    demo_basic_operations(database);
    demo_named_parameters(database);
    demo_transactions(database);
    demo_savepoints(database);
    demo_optional_columns(database);
    demo_utility_functions(database);

    spdlog::info("");
    spdlog::info("=== Demo Complete ===");
    spdlog::info("Next: Step 03 - Connection Pool");

  } catch (const db::DatabaseException &e) {
    spdlog::error("Database error ({}): {}", e.error_code(), e.what());
    return 1;
  } catch (const std::exception &e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }

  return 0;
}
