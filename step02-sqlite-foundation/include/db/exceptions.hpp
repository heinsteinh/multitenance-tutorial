#pragma once

#include <stdexcept>
#include <string>
#include <sqlite3.h>

namespace db {

/**
 * Base exception for all database errors
 */
class DatabaseException : public std::runtime_error {
public:
    DatabaseException(int error_code, const std::string& message)
        : std::runtime_error(message)
        , error_code_(error_code)
    {}

    int error_code() const noexcept { return error_code_; }

private:
    int error_code_;
};

/**
 * Thrown when a constraint is violated (UNIQUE, FOREIGN KEY, etc.)
 */
class ConstraintException : public DatabaseException {
public:
    ConstraintException(const std::string& message)
        : DatabaseException(SQLITE_CONSTRAINT, message)
    {}
};

/**
 * Thrown when the database is locked/busy
 */
class BusyException : public DatabaseException {
public:
    BusyException(const std::string& message)
        : DatabaseException(SQLITE_BUSY, message)
    {}
};

/**
 * Thrown when a query returns no results but one was expected
 */
class NotFoundException : public DatabaseException {
public:
    NotFoundException(const std::string& message)
        : DatabaseException(SQLITE_NOTFOUND, message)
    {}
};

/**
 * Thrown for type conversion errors
 */
class TypeException : public DatabaseException {
public:
    TypeException(const std::string& message)
        : DatabaseException(SQLITE_MISMATCH, message)
    {}
};

/**
 * Helper to create appropriate exception from SQLite error code
 */
inline void throw_sqlite_error(int error_code, const std::string& context, sqlite3* db = nullptr) {
    std::string message = context;
    if (db) {
        message += ": ";
        message += sqlite3_errmsg(db);
    }

    switch (error_code) {
        case SQLITE_CONSTRAINT:
        case SQLITE_CONSTRAINT_UNIQUE:
        case SQLITE_CONSTRAINT_PRIMARYKEY:
        case SQLITE_CONSTRAINT_FOREIGNKEY:
            throw ConstraintException(message);
        
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
            throw BusyException(message);
        
        default:
            throw DatabaseException(error_code, message);
    }
}

} // namespace db
