#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <functional>

namespace repository {

/**
 * Sort order for query results
 */
enum class SortOrder {
    Ascending,
    Descending
};

/**
 * A single WHERE condition
 */
struct WhereClause {
    std::string column;
    std::string op;  // =, !=, <, >, <=, >=, LIKE, IN, IS NULL, IS NOT NULL
    std::variant<
        std::monostate,  // For IS NULL / IS NOT NULL
        int64_t,
        double,
        std::string,
        std::vector<int64_t>,
        std::vector<std::string>
    > value;
};

/**
 * Sort specification
 */
struct OrderByClause {
    std::string column;
    SortOrder order = SortOrder::Ascending;
};

/**
 * Specification for querying entities.
 * 
 * Encapsulates query criteria in a reusable, composable way.
 * 
 * Usage:
 *   Specification<User> spec;
 *   spec.where("tenant_id", "=", tenant_id)
 *       .where("active", "=", true)
 *       .order_by("name")
 *       .limit(10);
 */
template<typename Entity>
class Specification {
public:
    Specification() = default;

    // ==================== WHERE Clauses ====================

    Specification& where(const std::string& column, const std::string& op, int64_t value) {
        where_clauses_.push_back({column, op, value});
        return *this;
    }

    Specification& where(const std::string& column, const std::string& op, double value) {
        where_clauses_.push_back({column, op, value});
        return *this;
    }

    Specification& where(const std::string& column, const std::string& op, const std::string& value) {
        where_clauses_.push_back({column, op, value});
        return *this;
    }

    Specification& where_null(const std::string& column) {
        where_clauses_.push_back({column, "IS NULL", std::monostate{}});
        return *this;
    }

    Specification& where_not_null(const std::string& column) {
        where_clauses_.push_back({column, "IS NOT NULL", std::monostate{}});
        return *this;
    }

    Specification& where_in(const std::string& column, const std::vector<int64_t>& values) {
        where_clauses_.push_back({column, "IN", values});
        return *this;
    }

    Specification& where_in(const std::string& column, const std::vector<std::string>& values) {
        where_clauses_.push_back({column, "IN", values});
        return *this;
    }

    Specification& where_like(const std::string& column, const std::string& pattern) {
        where_clauses_.push_back({column, "LIKE", pattern});
        return *this;
    }

    // ==================== ORDER BY ====================

    Specification& order_by(const std::string& column, SortOrder order = SortOrder::Ascending) {
        order_by_clauses_.push_back({column, order});
        return *this;
    }

    Specification& order_by_desc(const std::string& column) {
        return order_by(column, SortOrder::Descending);
    }

    // ==================== LIMIT / OFFSET ====================

    Specification& limit(size_t count) {
        limit_ = count;
        return *this;
    }

    Specification& offset(size_t count) {
        offset_ = count;
        return *this;
    }

    // ==================== Composition ====================

    /**
     * Combine with another specification (AND)
     */
    Specification& and_spec(const Specification& other) {
        for (const auto& clause : other.where_clauses_) {
            where_clauses_.push_back(clause);
        }
        return *this;
    }

    // ==================== Query Building ====================

    /**
     * Build the WHERE clause SQL
     * @return Pair of (SQL string, bind values)
     */
    std::string build_where_sql() const {
        if (where_clauses_.empty()) {
            return "";
        }

        std::string sql = " WHERE ";
        bool first = true;

        for (const auto& clause : where_clauses_) {
            if (!first) {
                sql += " AND ";
            }
            first = false;

            if (clause.op == "IS NULL" || clause.op == "IS NOT NULL") {
                sql += clause.column + " " + clause.op;
            } else if (clause.op == "IN") {
                sql += clause.column + " IN (";
                if (std::holds_alternative<std::vector<int64_t>>(clause.value)) {
                    const auto& values = std::get<std::vector<int64_t>>(clause.value);
                    for (size_t i = 0; i < values.size(); ++i) {
                        if (i > 0) sql += ", ";
                        sql += "?";
                    }
                } else if (std::holds_alternative<std::vector<std::string>>(clause.value)) {
                    const auto& values = std::get<std::vector<std::string>>(clause.value);
                    for (size_t i = 0; i < values.size(); ++i) {
                        if (i > 0) sql += ", ";
                        sql += "?";
                    }
                }
                sql += ")";
            } else {
                sql += clause.column + " " + clause.op + " ?";
            }
        }

        return sql;
    }

    /**
     * Build the ORDER BY clause SQL
     */
    std::string build_order_by_sql() const {
        if (order_by_clauses_.empty()) {
            return "";
        }

        std::string sql = " ORDER BY ";
        bool first = true;

        for (const auto& clause : order_by_clauses_) {
            if (!first) {
                sql += ", ";
            }
            first = false;
            sql += clause.column;
            sql += (clause.order == SortOrder::Descending) ? " DESC" : " ASC";
        }

        return sql;
    }

    /**
     * Build LIMIT/OFFSET SQL
     */
    std::string build_limit_sql() const {
        std::string sql;
        if (limit_) {
            sql += " LIMIT " + std::to_string(*limit_);
        }
        if (offset_) {
            sql += " OFFSET " + std::to_string(*offset_);
        }
        return sql;
    }

    /**
     * Get all bind values in order
     */
    template<typename Func>
    void bind_values(Func&& binder) const {
        int index = 1;
        for (const auto& clause : where_clauses_) {
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    // No binding for IS NULL / IS NOT NULL
                } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                    for (const auto& v : val) {
                        binder(index++, v);
                    }
                } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    for (const auto& v : val) {
                        binder(index++, v);
                    }
                } else {
                    binder(index++, val);
                }
            }, clause.value);
        }
    }

    // ==================== Accessors ====================

    const std::vector<WhereClause>& where_clauses() const { return where_clauses_; }
    const std::vector<OrderByClause>& order_by_clauses() const { return order_by_clauses_; }
    std::optional<size_t> get_limit() const { return limit_; }
    std::optional<size_t> get_offset() const { return offset_; }

private:
    std::vector<WhereClause> where_clauses_;
    std::vector<OrderByClause> order_by_clauses_;
    std::optional<size_t> limit_;
    std::optional<size_t> offset_;
};

} // namespace repository
