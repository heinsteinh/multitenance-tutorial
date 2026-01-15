#pragma once

#include "dto.hpp"
#include "exceptions.hpp"
#include "repository/user_repository.hpp"

#include <memory>
#include <vector>

namespace services
{

    /**
     * User service with repository-backed persistence
     */
    class UserService
    {
    public:
        /**
         * Construct with repository for database-backed storage
         */
        explicit UserService(std::shared_ptr<repository::UserRepository> repository);

        /**
         * Default constructor for in-memory storage (testing)
         */
        UserService();

        UserModel get_user(std::int64_t id) const;
        UserModel get_user_by_username(const std::string& tenant_id,
                                       const std::string& username) const;
        UserModel get_user_by_email(const std::string& email) const;
        std::vector<UserModel> list_users() const;
        std::vector<UserModel> list_users_by_tenant(const std::string& tenant_id) const;
        UserModel create_user(const CreateUserDto& dto);
        UserModel update_user(std::int64_t id, const UpdateUserDto& dto);
        void delete_user(std::int64_t id);

    private:
        std::shared_ptr<repository::UserRepository> repository_;

        // In-memory storage for testing mode
        bool use_memory_{false};
        mutable std::int64_t next_id_{1};
        mutable std::vector<UserModel> users_;

        // Helper for in-memory mode
        UserModel* find_user_by_id(std::int64_t id) const;
    };

} // namespace services
