#pragma once

#include <sqlite3.h>
#include <string>

namespace db {

// Forward declaration
class Database;

/**
 * RAII transaction wrapper.
 *
 * Automatically rolls back if commit() is not called before destruction.
 *
 * Usage:
 *   {
 *       auto tx = db.transaction();
 *       db.execute("INSERT ...");
 *       db.execute("UPDATE ...");
 *       tx.commit();  // Explicit commit required
 *   }  // Rolls back if commit() wasn't called
 */
class Transaction {
public:
  /**
   * Transaction types
   */
  enum class Type {
    Deferred,  // Lock acquired on first access (default)
    Immediate, // Write lock acquired immediately
    Exclusive  // Exclusive lock acquired immediately
  };

  /**
   * Begin a transaction
   * @param db Database handle
   * @param type Transaction type
   */
  explicit Transaction(sqlite3 *db, Type type = Type::Deferred);

  ~Transaction();

  // Move-only
  Transaction(Transaction &&other) noexcept;
  Transaction &operator=(Transaction &&other) noexcept;
  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;

  /**
   * Commit the transaction
   * @throws DatabaseException on failure
   */
  void commit();

  /**
   * Rollback the transaction (also called automatically on destruction)
   */
  void rollback();

  /**
   * Check if transaction is still active
   */
  bool is_active() const { return active_; }

private:
  sqlite3 *db_ = nullptr;
  bool active_ = false;

  void execute(const std::string &sql);
};

/**
 * Savepoint for nested transaction-like behavior.
 *
 * SQLite doesn't support nested transactions, but savepoints
 * provide similar functionality.
 *
 * Usage:
 *   auto tx = db.transaction();
 *   db.execute("INSERT ...");
 *   {
 *       Savepoint sp(db.handle(), "bulk_insert");
 *       for (...) {
 *           db.execute("INSERT ...");
 *       }
 *       sp.release();  // Or rollback to retry
 *   }
 *   tx.commit();
 */
class Savepoint {
public:
  /**
   * Create a savepoint
   * @param db Database handle
   * @param name Savepoint name (must be unique within transaction)
   */
  Savepoint(sqlite3 *db, const std::string &name);

  ~Savepoint();

  // Move-only
  Savepoint(Savepoint &&other) noexcept;
  Savepoint &operator=(Savepoint &&other) noexcept;
  Savepoint(const Savepoint &) = delete;
  Savepoint &operator=(const Savepoint &) = delete;

  /**
   * Release the savepoint (commits changes since savepoint)
   */
  void release();

  /**
   * Rollback to the savepoint
   */
  void rollback();

  bool is_active() const { return active_; }

private:
  sqlite3 *db_ = nullptr;
  std::string name_;
  bool active_ = false;

  void execute(const std::string &sql);
};

} // namespace db
