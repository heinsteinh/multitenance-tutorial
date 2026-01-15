#include "db/database.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace db
{

    // ==================== Transaction Implementation ====================

    Transaction::Transaction(sqlite3* db, Type type)
        : db_(db)
    {
        std::string sql;
        switch(type)
        {
        case Type::Deferred:
            sql = "BEGIN DEFERRED";
            break;
        case Type::Immediate:
            sql = "BEGIN IMMEDIATE";
            break;
        case Type::Exclusive:
            sql = "BEGIN EXCLUSIVE";
            break;
        }

        execute(sql);
        active_ = true;
        spdlog::trace("Transaction started");
    }

    Transaction::~Transaction()
    {
        if(active_)
        {
            try
            {
                rollback();
            }
            catch(const std::exception& e)
            {
                spdlog::error("Failed to rollback transaction: {}", e.what());
            }
        }
    }

    Transaction::Transaction(Transaction&& other) noexcept
        : db_(other.db_)
        , active_(other.active_)
    {
        other.db_     = nullptr;
        other.active_ = false;
    }

    Transaction& Transaction::operator=(Transaction&& other) noexcept
    {
        if(this != &other)
        {
            if(active_)
            {
                try
                {
                    rollback();
                }
                catch(...)
                {
                }
            }
            db_           = other.db_;
            active_       = other.active_;
            other.db_     = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    void Transaction::commit()
    {
        if(!active_)
        {
            throw DatabaseException(SQLITE_MISUSE, "Transaction not active");
        }
        execute("COMMIT");
        active_ = false;
        spdlog::trace("Transaction committed");
    }

    void Transaction::rollback()
    {
        if(!active_)
        {
            return;
        }
        execute("ROLLBACK");
        active_ = false;
        spdlog::trace("Transaction rolled back");
    }

    void Transaction::execute(const std::string& sql)
    {
        char* error_msg = nullptr;
        int result      = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

        if(result != SQLITE_OK)
        {
            std::string msg = error_msg ? error_msg : "Unknown error";
            sqlite3_free(error_msg);
            throw_sqlite_error(result, msg, db_);
        }
    }

    // ==================== Savepoint Implementation ====================

    Savepoint::Savepoint(sqlite3* db, const std::string& name)
        : db_(db)
        , name_(name)
    {
        execute(fmt::format("SAVEPOINT {}", name_));
        active_ = true;
        spdlog::trace("Savepoint '{}' created", name_);
    }

    Savepoint::~Savepoint()
    {
        if(active_)
        {
            try
            {
                rollback();
            }
            catch(const std::exception& e)
            {
                spdlog::error("Failed to rollback savepoint '{}': {}", name_, e.what());
            }
        }
    }

    Savepoint::Savepoint(Savepoint&& other) noexcept
        : db_(other.db_)
        , name_(std::move(other.name_))
        , active_(other.active_)
    {
        other.db_     = nullptr;
        other.active_ = false;
    }

    Savepoint& Savepoint::operator=(Savepoint&& other) noexcept
    {
        if(this != &other)
        {
            if(active_)
            {
                try
                {
                    rollback();
                }
                catch(...)
                {
                }
            }
            db_           = other.db_;
            name_         = std::move(other.name_);
            active_       = other.active_;
            other.db_     = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    void Savepoint::release()
    {
        if(!active_)
        {
            throw DatabaseException(SQLITE_MISUSE, "Savepoint not active");
        }
        execute(fmt::format("RELEASE SAVEPOINT {}", name_));
        active_ = false;
        spdlog::trace("Savepoint '{}' released", name_);
    }

    void Savepoint::rollback()
    {
        if(!active_)
        {
            return;
        }
        execute(fmt::format("ROLLBACK TO SAVEPOINT {}", name_));
        // After rollback, savepoint still exists - release it
        execute(fmt::format("RELEASE SAVEPOINT {}", name_));
        active_ = false;
        spdlog::trace("Savepoint '{}' rolled back", name_);
    }

    void Savepoint::execute(const std::string& sql)
    {
        char* error_msg = nullptr;
        int result      = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

        if(result != SQLITE_OK)
        {
            std::string msg = error_msg ? error_msg : "Unknown error";
            sqlite3_free(error_msg);
            throw_sqlite_error(result, msg, db_);
        }
    }

    // ==================== Database Implementation ====================

    Database::Database(const DatabaseConfig& config)
        : config_(config)
    {
        int flags = 0;

        if(config.read_only)
        {
            flags = SQLITE_OPEN_READONLY;
        }
        else
        {
            flags = SQLITE_OPEN_READWRITE;
            if(config.create_if_missing)
            {
                flags |= SQLITE_OPEN_CREATE;
            }
        }

        // Enable URI filenames and shared cache
        flags |= SQLITE_OPEN_URI;

        int result = sqlite3_open_v2(config.path.c_str(), &db_, flags, nullptr);

        if(result != SQLITE_OK)
        {
            std::string error_msg = db_ ? sqlite3_errmsg(db_) : "Unknown error";
            if(db_)
            {
                sqlite3_close(db_);
                db_ = nullptr;
            }
            throw DatabaseException(result,
                                    fmt::format("Failed to open database '{}': {}",
                                                config.path, error_msg));
        }

        apply_pragmas();

        spdlog::info("Opened database: {}", config.path);
    }

    Database::Database(const std::string& path)
        : Database(DatabaseConfig{.path = path})
    {
    }

    Database::~Database()
    {
        if(db_)
        {
            spdlog::debug("Closing database: {}", config_.path);
            sqlite3_close_v2(db_);
        }
    }

    Database::Database(Database&& other) noexcept
        : db_(other.db_)
        , config_(std::move(other.config_))
    {
        other.db_ = nullptr;
    }

    Database& Database::operator=(Database&& other) noexcept
    {
        if(this != &other)
        {
            if(db_)
            {
                sqlite3_close_v2(db_);
            }
            db_       = other.db_;
            config_   = std::move(other.config_);
            other.db_ = nullptr;
        }
        return *this;
    }

    void Database::apply_pragmas()
    {
        // Busy timeout
        execute(fmt::format("PRAGMA busy_timeout = {}", config_.busy_timeout_ms));

        // Foreign keys
        if(config_.enable_foreign_keys)
        {
            execute("PRAGMA foreign_keys = ON");
        }

        // WAL mode
        if(config_.enable_wal_mode && config_.path != ":memory:")
        {
            execute("PRAGMA journal_mode = WAL");
        }

        // Synchronous mode
        execute(fmt::format("PRAGMA synchronous = {}", config_.synchronous));

        spdlog::debug("Applied database pragmas");
    }

    void Database::execute(std::string_view sql)
    {
        char* error_msg = nullptr;
        int result =
            sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &error_msg);

        if(result != SQLITE_OK)
        {
            std::string msg = error_msg ? error_msg : "Unknown error";
            sqlite3_free(error_msg);
            throw_sqlite_error(result, fmt::format("Execute failed: {}", msg), db_);
        }

        spdlog::trace("Executed: {}", sql);
    }

    void Database::execute_script(std::string_view sql)
    {
        const char* current = sql.data();
        const char* end     = sql.data() + sql.size();

        while(current < end)
        {
            sqlite3_stmt* stmt = nullptr;
            const char* tail   = nullptr;

            int result = sqlite3_prepare_v2(
                db_, current, static_cast<int>(end - current), &stmt, &tail);

            if(result != SQLITE_OK)
            {
                throw_sqlite_error(result, "Failed to prepare script statement", db_);
            }

            if(stmt)
            {
                result = sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if(result != SQLITE_DONE && result != SQLITE_ROW)
                {
                    throw_sqlite_error(result, "Failed to execute script statement", db_);
                }
            }

            current = tail;

            // Skip whitespace
            while(current < end && std::isspace(*current))
            {
                ++current;
            }
        }
    }

    Statement Database::prepare(std::string_view sql)
    {
        return Statement(db_, sql);
    }

    Transaction Database::transaction(Transaction::Type type)
    {
        return Transaction(db_, type);
    }

    Savepoint Database::savepoint(const std::string& name)
    {
        return Savepoint(db_, name);
    }

    int64_t Database::last_insert_rowid() const
    {
        return sqlite3_last_insert_rowid(db_);
    }

    int Database::changes() const
    {
        return sqlite3_changes(db_);
    }

    int Database::total_changes() const
    {
        return sqlite3_total_changes(db_);
    }

    bool Database::is_autocommit() const
    {
        return sqlite3_get_autocommit(db_) != 0;
    }

    std::string Database::path() const
    {
        const char* db_path = sqlite3_db_filename(db_, "main");
        return db_path ? db_path : config_.path;
    }

    bool Database::table_exists(const std::string& table_name)
    {
        auto stmt = prepare(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?");
        stmt.bind(1, table_name);
        stmt.step();
        return stmt.column<int>(0) > 0;
    }

    std::string Database::sqlite_version()
    {
        return sqlite3_libversion();
    }

} // namespace db
