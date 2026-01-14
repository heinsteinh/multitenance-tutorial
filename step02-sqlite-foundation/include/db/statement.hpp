#pragma once

#include <sqlite3.h>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <cstdint>

namespace db {

/**
 * RAII wrapper for SQLite prepared statements.
 * 
 * Usage:
 *   Statement stmt(db, "SELECT * FROM users WHERE id = ?");
 *   stmt.bind(1, user_id);
 *   while (stmt.step()) {
 *       auto name = stmt.column<std::string>(1);
 *   }
 */
class Statement {
public:
    /**
     * Prepare a SQL statement
     * @param db SQLite database handle
     * @param sql SQL query with optional ? or :name placeholders
     */
    Statement(sqlite3* db, std::string_view sql);
    
    ~Statement();

    // Move-only
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // ==================== Binding ====================

    /**
     * Bind integer value to parameter
     * @param index 1-based parameter index
     * @param value Value to bind
     */
    Statement& bind(int index, int value);
    Statement& bind(int index, int64_t value);
    Statement& bind(int index, double value);
    Statement& bind(int index, std::string_view value);
    Statement& bind(int index, const std::vector<uint8_t>& blob);
    Statement& bind(int index, std::nullptr_t);

    /**
     * Bind to named parameter (e.g., :name, @name, $name)
     */
    Statement& bind(const std::string& name, int value);
    Statement& bind(const std::string& name, int64_t value);
    Statement& bind(const std::string& name, double value);
    Statement& bind(const std::string& name, std::string_view value);
    Statement& bind(const std::string& name, const std::vector<uint8_t>& blob);
    Statement& bind(const std::string& name, std::nullptr_t);

    /**
     * Bind optional value - binds NULL if not present
     */
    template<typename T>
    Statement& bind(int index, const std::optional<T>& value) {
        if (value.has_value()) {
            return bind(index, *value);
        }
        return bind(index, nullptr);
    }

    template<typename T>
    Statement& bind(const std::string& name, const std::optional<T>& value) {
        if (value.has_value()) {
            return bind(name, *value);
        }
        return bind(name, nullptr);
    }

    // ==================== Execution ====================

    /**
     * Execute one step of the statement
     * @return true if a row is available, false if done
     */
    bool step();

    /**
     * Reset the statement for re-execution with new bindings
     */
    void reset();

    /**
     * Clear all bindings
     */
    void clear_bindings();

    // ==================== Column Access ====================

    /**
     * Get number of columns in result
     */
    int column_count() const;

    /**
     * Get column name
     */
    std::string column_name(int index) const;

    /**
     * Get column type
     */
    int column_type(int index) const;

    /**
     * Check if column is NULL
     */
    bool is_null(int index) const;

    /**
     * Get column value as specific type
     * Template specializations provided for:
     * - int, int64_t, double
     * - std::string
     * - std::vector<uint8_t>
     */
    template<typename T>
    T column(int index) const;

    /**
     * Get nullable column value
     */
    template<typename T>
    std::optional<T> column_optional(int index) const {
        if (is_null(index)) {
            return std::nullopt;
        }
        return column<T>(index);
    }

    /**
     * Get underlying SQLite statement handle
     */
    sqlite3_stmt* handle() const { return stmt_; }

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
    
    int get_param_index(const std::string& name) const;
    void check_bind_result(int result, int index);
};

// Template specializations declared here, defined in .cpp
template<> int Statement::column<int>(int index) const;
template<> int64_t Statement::column<int64_t>(int index) const;
template<> double Statement::column<double>(int index) const;
template<> std::string Statement::column<std::string>(int index) const;
template<> std::vector<uint8_t> Statement::column<std::vector<uint8_t>>(int index) const;

} // namespace db
