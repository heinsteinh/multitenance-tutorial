#pragma once

#include "repository/repository.hpp"
#include <optional>

namespace repository {

/**
 * User entity for the multi-tenant system
 */
struct User {
    int64_t id = 0;
    std::string tenant_id;
    std::string username;
    std::string email;
    std::string password_hash;
    std::string role;
    bool active = true;
    std::string created_at;
    std::string updated_at;

    static constexpr std::string_view table_name = "users";
    static constexpr std::string_view primary_key = "id";
};

/**
 * Tenant entity
 */
struct Tenant {
    int64_t id = 0;
    std::string tenant_id;  // Unique string identifier (slug)
    std::string name;
    std::string plan;       // free, basic, pro, enterprise
    bool active = true;
    std::string db_path;    // Path to tenant's database
    std::string created_at;
    std::string updated_at;

    static constexpr std::string_view table_name = "tenants";
    static constexpr std::string_view primary_key = "id";
};

/**
 * Permission entity
 */
struct Permission {
    int64_t id = 0;
    std::string tenant_id;
    int64_t user_id;
    std::string resource;
    std::string action;  // create, read, update, delete
    bool allowed = true;
    std::string created_at;

    static constexpr std::string_view table_name = "permissions";
    static constexpr std::string_view primary_key = "id";
};

// ==================== Concrete Repositories ====================

/**
 * User repository with complete CRUD implementation
 */
class UserRepository : public Repository<User> {
public:
    using Repository<User>::Repository;

    // Additional user-specific queries

    std::optional<User> find_by_email(const std::string& email) {
        Specification<User> spec;
        spec.where("email", "=", email);
        return find_one(spec);
    }

    std::optional<User> find_by_username(const std::string& tenant_id, const std::string& username) {
        Specification<User> spec;
        spec.where("tenant_id", "=", tenant_id)
            .where("username", "=", username);
        return find_one(spec);
    }

    std::vector<User> find_by_tenant(const std::string& tenant_id) {
        Specification<User> spec;
        spec.where("tenant_id", "=", tenant_id)
            .order_by("username");
        return find_by(spec);
    }

    std::vector<User> find_active_by_tenant(const std::string& tenant_id) {
        Specification<User> spec;
        spec.where("tenant_id", "=", tenant_id)
            .where("active", "=", static_cast<int64_t>(1))
            .order_by("username");
        return find_by(spec);
    }

    size_t count_by_tenant(const std::string& tenant_id) {
        Specification<User> spec;
        spec.where("tenant_id", "=", tenant_id);
        return count_by(spec);
    }

protected:
    std::string get_select_columns() const override {
        return "id, tenant_id, username, email, password_hash, role, active, created_at, updated_at";
    }

    std::string get_insert_columns() const override {
        return "tenant_id, username, email, password_hash, role, active, created_at, updated_at";
    }

    std::string get_insert_placeholders() const override {
        return "?, ?, ?, ?, ?, ?, datetime('now'), datetime('now')";
    }

    std::string get_update_set_clause() const override {
        return "tenant_id = ?, username = ?, email = ?, password_hash = ?, role = ?, active = ?, updated_at = datetime('now')";
    }

    User map_from_row(db::Statement& stmt) const override {
        return User{
            .id = stmt.column<int64_t>(0),
            .tenant_id = stmt.column<std::string>(1),
            .username = stmt.column<std::string>(2),
            .email = stmt.column<std::string>(3),
            .password_hash = stmt.column<std::string>(4),
            .role = stmt.column<std::string>(5),
            .active = stmt.column<int64_t>(6) != 0,
            .created_at = stmt.column<std::string>(7),
            .updated_at = stmt.column<std::string>(8)
        };
    }

    void bind_insert_values(db::Statement& stmt, const User& user) const override {
        stmt.bind(1, user.tenant_id);
        stmt.bind(2, user.username);
        stmt.bind(3, user.email);
        stmt.bind(4, user.password_hash);
        stmt.bind(5, user.role);
        stmt.bind(6, user.active ? 1 : 0);
    }

    int bind_update_values(db::Statement& stmt, const User& user) const override {
        stmt.bind(1, user.tenant_id);
        stmt.bind(2, user.username);
        stmt.bind(3, user.email);
        stmt.bind(4, user.password_hash);
        stmt.bind(5, user.role);
        stmt.bind(6, user.active ? 1 : 0);
        return 7;  // Next parameter index
    }
};

/**
 * Tenant repository
 */
class TenantRepository : public Repository<Tenant> {
public:
    using Repository<Tenant>::Repository;

    std::optional<Tenant> find_by_tenant_id(const std::string& tenant_id) {
        Specification<Tenant> spec;
        spec.where("tenant_id", "=", tenant_id);
        return find_one(spec);
    }

    std::vector<Tenant> find_active() {
        Specification<Tenant> spec;
        spec.where("active", "=", static_cast<int64_t>(1))
            .order_by("name");
        return find_by(spec);
    }

    std::vector<Tenant> find_by_plan(const std::string& plan) {
        Specification<Tenant> spec;
        spec.where("plan", "=", plan)
            .order_by("name");
        return find_by(spec);
    }

protected:
    std::string get_select_columns() const override {
        return "id, tenant_id, name, plan, active, db_path, created_at, updated_at";
    }

    std::string get_insert_columns() const override {
        return "tenant_id, name, plan, active, db_path, created_at, updated_at";
    }

    std::string get_insert_placeholders() const override {
        return "?, ?, ?, ?, ?, datetime('now'), datetime('now')";
    }

    std::string get_update_set_clause() const override {
        return "tenant_id = ?, name = ?, plan = ?, active = ?, db_path = ?, updated_at = datetime('now')";
    }

    Tenant map_from_row(db::Statement& stmt) const override {
        return Tenant{
            .id = stmt.column<int64_t>(0),
            .tenant_id = stmt.column<std::string>(1),
            .name = stmt.column<std::string>(2),
            .plan = stmt.column<std::string>(3),
            .active = stmt.column<int64_t>(4) != 0,
            .db_path = stmt.column<std::string>(5),
            .created_at = stmt.column<std::string>(6),
            .updated_at = stmt.column<std::string>(7)
        };
    }

    void bind_insert_values(db::Statement& stmt, const Tenant& tenant) const override {
        stmt.bind(1, tenant.tenant_id);
        stmt.bind(2, tenant.name);
        stmt.bind(3, tenant.plan);
        stmt.bind(4, tenant.active ? 1 : 0);
        stmt.bind(5, tenant.db_path);
    }

    int bind_update_values(db::Statement& stmt, const Tenant& tenant) const override {
        stmt.bind(1, tenant.tenant_id);
        stmt.bind(2, tenant.name);
        stmt.bind(3, tenant.plan);
        stmt.bind(4, tenant.active ? 1 : 0);
        stmt.bind(5, tenant.db_path);
        return 6;
    }
};

/**
 * Permission repository
 */
class PermissionRepository : public Repository<Permission> {
public:
    using Repository<Permission>::Repository;

    std::vector<Permission> find_by_user(const std::string& tenant_id, int64_t user_id) {
        Specification<Permission> spec;
        spec.where("tenant_id", "=", tenant_id)
            .where("user_id", "=", user_id);
        return find_by(spec);
    }

    bool has_permission(const std::string& tenant_id, int64_t user_id,
                       const std::string& resource, const std::string& action) {
        Specification<Permission> spec;
        spec.where("tenant_id", "=", tenant_id)
            .where("user_id", "=", user_id)
            .where("resource", "=", resource)
            .where("action", "=", action)
            .where("allowed", "=", static_cast<int64_t>(1));
        return exists(spec);
    }

protected:
    std::string get_select_columns() const override {
        return "id, tenant_id, user_id, resource, action, allowed, created_at";
    }

    std::string get_insert_columns() const override {
        return "tenant_id, user_id, resource, action, allowed, created_at";
    }

    std::string get_insert_placeholders() const override {
        return "?, ?, ?, ?, ?, datetime('now')";
    }

    std::string get_update_set_clause() const override {
        return "tenant_id = ?, user_id = ?, resource = ?, action = ?, allowed = ?";
    }

    Permission map_from_row(db::Statement& stmt) const override {
        return Permission{
            .id = stmt.column<int64_t>(0),
            .tenant_id = stmt.column<std::string>(1),
            .user_id = stmt.column<int64_t>(2),
            .resource = stmt.column<std::string>(3),
            .action = stmt.column<std::string>(4),
            .allowed = stmt.column<int64_t>(5) != 0,
            .created_at = stmt.column<std::string>(6)
        };
    }

    void bind_insert_values(db::Statement& stmt, const Permission& perm) const override {
        stmt.bind(1, perm.tenant_id);
        stmt.bind(2, perm.user_id);
        stmt.bind(3, perm.resource);
        stmt.bind(4, perm.action);
        stmt.bind(5, perm.allowed ? 1 : 0);
    }

    int bind_update_values(db::Statement& stmt, const Permission& perm) const override {
        stmt.bind(1, perm.tenant_id);
        stmt.bind(2, perm.user_id);
        stmt.bind(3, perm.resource);
        stmt.bind(4, perm.action);
        stmt.bind(5, perm.allowed ? 1 : 0);
        return 6;
    }
};

} // namespace repository
