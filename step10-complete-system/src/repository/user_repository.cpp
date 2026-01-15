#include "repository/user_repository.hpp"

#include <spdlog/spdlog.h>

namespace repository
{

    UserRepository::UserRepository(std::shared_ptr<db::Database> database)
        : database_(std::move(database))
    {
    }

    void UserRepository::initialize_schema()
    {
        if(!database_->table_exists("users"))
        {
            database_->execute(R"(
                CREATE TABLE users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    tenant_id TEXT NOT NULL,
                    username TEXT NOT NULL,
                    email TEXT NOT NULL UNIQUE,
                    password_hash TEXT,
                    role TEXT DEFAULT 'user',
                    active INTEGER DEFAULT 1,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(tenant_id, username)
                )
            )");

            database_->execute(
                "CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id)");
            database_->execute(
                "CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)");

            spdlog::info("Created users table");
        }
    }

    std::optional<services::UserModel> UserRepository::find_by_id(int64_t id)
    {
        auto stmt = database_->prepare(
            "SELECT id, tenant_id, username, email, password_hash, role, active "
            "FROM users WHERE id = ?");
        stmt.bind(1, id);

        if(stmt.step())
        {
            return map_from_row(stmt);
        }
        return std::nullopt;
    }

    std::optional<services::UserModel> UserRepository::find_by_email(
        const std::string& email)
    {
        auto stmt = database_->prepare(
            "SELECT id, tenant_id, username, email, password_hash, role, active "
            "FROM users WHERE email = ?");
        stmt.bind(1, email);

        if(stmt.step())
        {
            return map_from_row(stmt);
        }
        return std::nullopt;
    }

    std::optional<services::UserModel> UserRepository::find_by_username(
        const std::string& tenant_id, const std::string& username)
    {
        auto stmt = database_->prepare(
            "SELECT id, tenant_id, username, email, password_hash, role, active "
            "FROM users WHERE tenant_id = ? AND username = ?");
        stmt.bind(1, tenant_id);
        stmt.bind(2, username);

        if(stmt.step())
        {
            return map_from_row(stmt);
        }
        return std::nullopt;
    }

    std::vector<services::UserModel> UserRepository::find_by_tenant(
        const std::string& tenant_id)
    {
        std::vector<services::UserModel> users;

        auto stmt = database_->prepare(
            "SELECT id, tenant_id, username, email, password_hash, role, active "
            "FROM users WHERE tenant_id = ? ORDER BY id");
        stmt.bind(1, tenant_id);

        while(stmt.step())
        {
            users.push_back(map_from_row(stmt));
        }

        return users;
    }

    std::vector<services::UserModel> UserRepository::find_all()
    {
        std::vector<services::UserModel> users;

        auto stmt = database_->prepare(
            "SELECT id, tenant_id, username, email, password_hash, role, active "
            "FROM users ORDER BY id");

        while(stmt.step())
        {
            users.push_back(map_from_row(stmt));
        }

        return users;
    }

    int64_t UserRepository::insert(const services::UserModel& user)
    {
        auto stmt = database_->prepare(
            "INSERT INTO users (tenant_id, username, email, password_hash, role, active) "
            "VALUES (?, ?, ?, ?, ?, ?)");

        stmt.bind(1, user.tenant_id);
        stmt.bind(2, user.username);
        stmt.bind(3, user.email);
        stmt.bind(4, user.password_hash);
        stmt.bind(5, user.role);
        stmt.bind(6, user.active ? 1 : 0);
        stmt.step();

        int64_t id = database_->last_insert_rowid();
        spdlog::debug("Inserted user {} with id {}", user.username, id);
        return id;
    }

    void UserRepository::update(const services::UserModel& user)
    {
        auto stmt = database_->prepare(
            "UPDATE users SET tenant_id = ?, username = ?, email = ?, "
            "password_hash = ?, role = ?, active = ?, updated_at = CURRENT_TIMESTAMP "
            "WHERE id = ?");

        stmt.bind(1, user.tenant_id);
        stmt.bind(2, user.username);
        stmt.bind(3, user.email);
        stmt.bind(4, user.password_hash);
        stmt.bind(5, user.role);
        stmt.bind(6, user.active ? 1 : 0);
        stmt.bind(7, user.id);
        stmt.step();

        spdlog::debug("Updated user {}", user.id);
    }

    void UserRepository::remove(int64_t id)
    {
        auto stmt = database_->prepare("DELETE FROM users WHERE id = ?");
        stmt.bind(1, id);
        stmt.step();

        spdlog::debug("Deleted user {}", id);
    }

    size_t UserRepository::count_by_tenant(const std::string& tenant_id)
    {
        auto stmt =
            database_->prepare("SELECT COUNT(*) FROM users WHERE tenant_id = ?");
        stmt.bind(1, tenant_id);
        stmt.step();
        return static_cast<size_t>(stmt.column<int64_t>(0));
    }

    bool UserRepository::email_exists(const std::string& email)
    {
        auto stmt =
            database_->prepare("SELECT COUNT(*) FROM users WHERE email = ?");
        stmt.bind(1, email);
        stmt.step();
        return stmt.column<int>(0) > 0;
    }

    bool UserRepository::username_exists(const std::string& tenant_id,
                                         const std::string& username)
    {
        auto stmt = database_->prepare(
            "SELECT COUNT(*) FROM users WHERE tenant_id = ? AND username = ?");
        stmt.bind(1, tenant_id);
        stmt.bind(2, username);
        stmt.step();
        return stmt.column<int>(0) > 0;
    }

    services::UserModel UserRepository::map_from_row(db::Statement& stmt)
    {
        services::UserModel user;
        user.id            = stmt.column<int64_t>(0);
        user.tenant_id     = stmt.column<std::string>(1);
        user.username      = stmt.column<std::string>(2);
        user.email         = stmt.column<std::string>(3);
        user.password_hash = stmt.column<std::string>(4);
        user.role          = stmt.column<std::string>(5);
        user.active        = stmt.column<int>(6) != 0;
        return user;
    }

} // namespace repository
