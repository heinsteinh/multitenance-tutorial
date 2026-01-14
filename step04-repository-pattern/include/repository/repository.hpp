#pragma once

#include "db/database.hpp"
#include "pool/connection_pool.hpp"
#include "repository/entity.hpp"
#include "repository/specification.hpp"

#include <fmt/format.h>
#include <functional>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <vector>

namespace repository {

/**
 * Generic repository for CRUD operations on entities.
 *
 * Usage:
 *   Repository<User> userRepo(connectionPool);
 *
 *   auto user = userRepo.find_by_id(123);
 *   auto users = userRepo.find_all();
 *
 *   User newUser{.name = "Alice", .email = "alice@example.com"};
 *   int64_t id = userRepo.insert(newUser);
 *
 *   userRepo.remove(id);
 *
 * @tparam Entity Type that satisfies the Entity concept
 */
template <typename T> class Repository {
public:
  using EntityType = T;
  using SpecificationType = Specification<T>;

  /**
   * Create repository with connection pool
   */
  explicit Repository(pool::ConnectionPool &pool) : pool_(&pool) {}

  /**
   * Create repository with database connection (for single-connection use)
   */
  explicit Repository(db::Database &db) : db_(&db) {}

  // ==================== Find Operations ====================

  /**
   * Find entity by primary key
   */
  std::optional<T> find_by_id(int64_t id) {
    auto conn = get_connection();

    std::string sql =
        fmt::format("SELECT {} FROM {} WHERE {} = ?", get_select_columns(),
                    T::table_name, T::primary_key);

    auto stmt = conn->prepare(sql);
    stmt.bind(1, id);

    if (stmt.step()) {
      return map_from_row(stmt);
    }
    return std::nullopt;
  }

  /**
   * Find all entities
   */
  std::vector<T> find_all() { return find_by(SpecificationType{}); }

  /**
   * Find entities matching specification
   */
  std::vector<T> find_by(const SpecificationType &spec) {
    auto conn = get_connection();

    std::string sql =
        fmt::format("SELECT {} FROM {}{}{}{}", get_select_columns(),
                    T::table_name, spec.build_where_sql(),
                    spec.build_order_by_sql(), spec.build_limit_sql());

    auto stmt = conn->prepare(sql);
    spec.bind_values(
        [&stmt](int index, const auto &value) { stmt.bind(index, value); });

    std::vector<T> results;
    while (stmt.step()) {
      results.push_back(map_from_row(stmt));
    }
    return results;
  }

  /**
   * Find first entity matching specification
   */
  std::optional<T> find_one(const SpecificationType &spec) {
    auto limited = spec;
    limited.limit(1);

    auto results = find_by(limited);
    if (!results.empty()) {
      return results[0];
    }
    return std::nullopt;
  }

  // ==================== Count Operations ====================

  /**
   * Count all entities
   */
  size_t count() { return count_by(SpecificationType{}); }

  /**
   * Count entities matching specification
   */
  size_t count_by(const SpecificationType &spec) {
    auto conn = get_connection();

    std::string sql = fmt::format("SELECT COUNT(*) FROM {}{}", T::table_name,
                                  spec.build_where_sql());

    auto stmt = conn->prepare(sql);
    spec.bind_values(
        [&stmt](int index, const auto &value) { stmt.bind(index, value); });

    if (stmt.step()) {
      return static_cast<size_t>(stmt.template column<int64_t>(0));
    }
    return 0;
  }

  /**
   * Check if any entity matches specification
   */
  bool exists(const SpecificationType &spec) { return count_by(spec) > 0; }

  // ==================== Insert Operations ====================

  /**
   * Insert a new entity
   * @return The auto-generated ID
   */
  int64_t insert(const T &entity) {
    auto conn = get_connection();

    std::string sql =
        fmt::format("INSERT INTO {} ({}) VALUES ({})", T::table_name,
                    get_insert_columns(), get_insert_placeholders());

    auto stmt = conn->prepare(sql);
    bind_insert_values(stmt, entity);
    stmt.step();

    return conn->last_insert_rowid();
  }

  /**
   * Insert multiple entities in a batch
   * @return Vector of generated IDs
   */
  std::vector<int64_t> insert_batch(const std::vector<T> &entities) {
    if (entities.empty()) {
      return {};
    }

    auto conn = get_connection();
    auto tx = conn->transaction();

    std::string sql =
        fmt::format("INSERT INTO {} ({}) VALUES ({})", T::table_name,
                    get_insert_columns(), get_insert_placeholders());

    auto stmt = conn->prepare(sql);
    std::vector<int64_t> ids;
    ids.reserve(entities.size());

    for (const auto &entity : entities) {
      bind_insert_values(stmt, entity);
      stmt.step();
      ids.push_back(conn->last_insert_rowid());
      stmt.reset();
      stmt.clear_bindings();
    }

    tx.commit();
    return ids;
  }

  // ==================== Update Operations ====================

  /**
   * Update an existing entity
   */
  void update(const T &entity) {
    auto conn = get_connection();

    std::string sql =
        fmt::format("UPDATE {} SET {} WHERE {} = ?", T::table_name,
                    get_update_set_clause(), T::primary_key);

    auto stmt = conn->prepare(sql);
    int param_index = bind_update_values(stmt, entity);
    stmt.bind(param_index, entity.id);
    stmt.step();
  }

  // ==================== Delete Operations ====================

  /**
   * Remove entity by ID
   */
  void remove(int64_t id) {
    auto conn = get_connection();

    std::string sql = fmt::format("DELETE FROM {} WHERE {} = ?", T::table_name,
                                  T::primary_key);

    auto stmt = conn->prepare(sql);
    stmt.bind(1, id);
    stmt.step();
  }

  /**
   * Remove entities matching specification
   * @return Number of entities removed
   */
  size_t remove_by(const SpecificationType &spec) {
    auto conn = get_connection();

    std::string sql =
        fmt::format("DELETE FROM {}{}", T::table_name, spec.build_where_sql());

    auto stmt = conn->prepare(sql);
    spec.bind_values(
        [&stmt](int index, const auto &value) { stmt.bind(index, value); });
    stmt.step();

    return static_cast<size_t>(conn->changes());
  }

  /**
   * Remove all entities
   * @return Number of entities removed
   */
  size_t remove_all() {
    auto conn = get_connection();
    conn->execute(fmt::format("DELETE FROM {}", T::table_name));
    return static_cast<size_t>(conn->changes());
  }

protected:
  // These methods should be overridden or specialized for each entity type

  virtual std::string get_select_columns() const = 0;
  virtual std::string get_insert_columns() const = 0;
  virtual std::string get_insert_placeholders() const = 0;
  virtual std::string get_update_set_clause() const = 0;

  virtual T map_from_row(db::Statement &stmt) const = 0;
  virtual void bind_insert_values(db::Statement &stmt,
                                  const T &entity) const = 0;
  virtual int bind_update_values(db::Statement &stmt,
                                 const T &entity) const = 0;

  /**
   * Get a database connection (from pool or direct)
   */
  class ConnectionHandle {
  public:
    ConnectionHandle(pool::PooledConnection conn)
        : pooled_(std::move(conn)), db_(pooled_->get()) {}
    ConnectionHandle(db::Database *db) : db_(db) {}

    db::Database *operator->() { return db_; }
    db::Database &operator*() { return *db_; }

  private:
    std::optional<pool::PooledConnection> pooled_;
    db::Database *db_;
  };

  ConnectionHandle get_connection() {
    if (db_) {
      return ConnectionHandle(db_);
    }
    return ConnectionHandle(pool_->acquire());
  }

private:
  pool::ConnectionPool *pool_ = nullptr;
  db::Database *db_ = nullptr;
};

} // namespace repository
