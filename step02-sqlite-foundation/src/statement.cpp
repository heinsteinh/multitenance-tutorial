#include "db/statement.hpp"
#include "db/exceptions.hpp"

#include <spdlog/spdlog.h>
#include <cstring>

namespace db {

Statement::Statement(sqlite3* db, std::string_view sql)
    : db_(db)
{
    const char* tail = nullptr;
    int result = sqlite3_prepare_v2(
        db_,
        sql.data(),
        static_cast<int>(sql.size()),
        &stmt_,
        &tail
    );

    if (result != SQLITE_OK) {
        throw_sqlite_error(result, "Failed to prepare statement", db_);
    }

    spdlog::trace("Prepared statement: {}", sql);
}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

Statement::Statement(Statement&& other) noexcept
    : db_(other.db_)
    , stmt_(other.stmt_)
{
    other.db_ = nullptr;
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
        db_ = other.db_;
        stmt_ = other.stmt_;
        other.db_ = nullptr;
        other.stmt_ = nullptr;
    }
    return *this;
}

// ==================== Binding by Index ====================

Statement& Statement::bind(int index, int value) {
    check_bind_result(sqlite3_bind_int(stmt_, index, value), index);
    return *this;
}

Statement& Statement::bind(int index, int64_t value) {
    check_bind_result(sqlite3_bind_int64(stmt_, index, value), index);
    return *this;
}

Statement& Statement::bind(int index, double value) {
    check_bind_result(sqlite3_bind_double(stmt_, index, value), index);
    return *this;
}

Statement& Statement::bind(int index, std::string_view value) {
    // SQLITE_TRANSIENT makes SQLite copy the string
    check_bind_result(
        sqlite3_bind_text(stmt_, index, value.data(), 
                         static_cast<int>(value.size()), SQLITE_TRANSIENT),
        index
    );
    return *this;
}

Statement& Statement::bind(int index, const std::vector<uint8_t>& blob) {
    check_bind_result(
        sqlite3_bind_blob(stmt_, index, blob.data(), 
                         static_cast<int>(blob.size()), SQLITE_TRANSIENT),
        index
    );
    return *this;
}

Statement& Statement::bind(int index, std::nullptr_t) {
    check_bind_result(sqlite3_bind_null(stmt_, index), index);
    return *this;
}

// ==================== Binding by Name ====================

Statement& Statement::bind(const std::string& name, int value) {
    return bind(get_param_index(name), value);
}

Statement& Statement::bind(const std::string& name, int64_t value) {
    return bind(get_param_index(name), value);
}

Statement& Statement::bind(const std::string& name, double value) {
    return bind(get_param_index(name), value);
}

Statement& Statement::bind(const std::string& name, std::string_view value) {
    return bind(get_param_index(name), value);
}

Statement& Statement::bind(const std::string& name, const std::vector<uint8_t>& blob) {
    return bind(get_param_index(name), blob);
}

Statement& Statement::bind(const std::string& name, std::nullptr_t) {
    return bind(get_param_index(name), nullptr);
}

// ==================== Execution ====================

bool Statement::step() {
    int result = sqlite3_step(stmt_);
    
    if (result == SQLITE_ROW) {
        return true;
    }
    
    if (result == SQLITE_DONE) {
        return false;
    }
    
    throw_sqlite_error(result, "Statement step failed", db_);
    return false;  // Never reached
}

void Statement::reset() {
    int result = sqlite3_reset(stmt_);
    if (result != SQLITE_OK) {
        throw_sqlite_error(result, "Failed to reset statement", db_);
    }
}

void Statement::clear_bindings() {
    int result = sqlite3_clear_bindings(stmt_);
    if (result != SQLITE_OK) {
        throw_sqlite_error(result, "Failed to clear bindings", db_);
    }
}

// ==================== Column Access ====================

int Statement::column_count() const {
    return sqlite3_column_count(stmt_);
}

std::string Statement::column_name(int index) const {
    const char* name = sqlite3_column_name(stmt_, index);
    return name ? name : "";
}

int Statement::column_type(int index) const {
    return sqlite3_column_type(stmt_, index);
}

bool Statement::is_null(int index) const {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

template<>
int Statement::column<int>(int index) const {
    return sqlite3_column_int(stmt_, index);
}

template<>
int64_t Statement::column<int64_t>(int index) const {
    return sqlite3_column_int64(stmt_, index);
}

template<>
double Statement::column<double>(int index) const {
    return sqlite3_column_double(stmt_, index);
}

template<>
std::string Statement::column<std::string>(int index) const {
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    int size = sqlite3_column_bytes(stmt_, index);
    
    if (text && size > 0) {
        return std::string(reinterpret_cast<const char*>(text), size);
    }
    return "";
}

template<>
std::vector<uint8_t> Statement::column<std::vector<uint8_t>>(int index) const {
    const void* data = sqlite3_column_blob(stmt_, index);
    int size = sqlite3_column_bytes(stmt_, index);
    
    if (data && size > 0) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(bytes, bytes + size);
    }
    return {};
}

// ==================== Private Helpers ====================

int Statement::get_param_index(const std::string& name) const {
    int index = sqlite3_bind_parameter_index(stmt_, name.c_str());
    if (index == 0) {
        throw DatabaseException(SQLITE_ERROR, 
            "Unknown parameter name: " + name);
    }
    return index;
}

void Statement::check_bind_result(int result, int index) {
    if (result != SQLITE_OK) {
        throw_sqlite_error(result, 
            "Failed to bind parameter " + std::to_string(index), db_);
    }
}

} // namespace db
