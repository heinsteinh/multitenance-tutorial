#include "services/user_service.hpp"

#include <algorithm>

namespace services
{

    UserService::UserService(std::shared_ptr<repository::UserRepository> repository)
        : repository_(std::move(repository))
        , use_memory_(false)
    {
    }

    UserService::UserService()
        : repository_(nullptr)
        , use_memory_(true)
    {
        // seed with a default user for demo purposes
        UserModel admin{.id         = next_id_++,
                        .tenant_id  = "demo",
                        .username   = "admin",
                        .email      = "admin@example.com",
                        .password_hash = "",
                        .role       = "admin",
                        .active     = true};
        users_.push_back(admin);
    }

    UserModel* UserService::find_user_by_id(std::int64_t id) const
    {
        auto it = std::find_if(
            users_.begin(), users_.end(), [&](const UserModel& u) { return u.id == id; });
        if(it == users_.end())
        {
            return nullptr;
        }
        return &(*it);
    }

    UserModel UserService::get_user(std::int64_t id) const
    {
        if(use_memory_)
        {
            auto* user = find_user_by_id(id);
            if(!user)
            {
                throw NotFoundException("User not found");
            }
            return *user;
        }

        auto user = repository_->find_by_id(id);
        if(!user)
        {
            throw NotFoundException("User not found");
        }
        return user.value();
    }

    UserModel UserService::get_user_by_username(const std::string& tenant_id,
                                                const std::string& username) const
    {
        if(use_memory_)
        {
            auto it =
                std::find_if(users_.begin(), users_.end(), [&](const UserModel& u) {
                    return u.tenant_id == tenant_id && u.username == username;
                });
            if(it == users_.end())
            {
                throw NotFoundException("User not found");
            }
            return *it;
        }

        auto user = repository_->find_by_username(tenant_id, username);
        if(!user)
        {
            throw NotFoundException("User not found");
        }
        return user.value();
    }

    UserModel UserService::get_user_by_email(const std::string& email) const
    {
        if(use_memory_)
        {
            auto it = std::find_if(users_.begin(), users_.end(),
                                   [&](const UserModel& u) { return u.email == email; });
            if(it == users_.end())
            {
                throw NotFoundException("User not found");
            }
            return *it;
        }

        auto user = repository_->find_by_email(email);
        if(!user)
        {
            throw NotFoundException("User not found");
        }
        return user.value();
    }

    std::vector<UserModel> UserService::list_users() const
    {
        if(use_memory_)
        {
            return users_;
        }

        return repository_->find_all();
    }

    std::vector<UserModel> UserService::list_users_by_tenant(
        const std::string& tenant_id) const
    {
        if(use_memory_)
        {
            std::vector<UserModel> result;
            std::copy_if(users_.begin(), users_.end(), std::back_inserter(result),
                         [&](const UserModel& u) { return u.tenant_id == tenant_id; });
            return result;
        }

        return repository_->find_by_tenant(tenant_id);
    }

    UserModel UserService::create_user(const CreateUserDto& dto)
    {
        if(dto.username.empty())
        {
            throw ValidationException("Username is required");
        }
        if(dto.email.empty())
        {
            throw ValidationException("Email is required");
        }

        if(use_memory_)
        {
            auto exists =
                std::any_of(users_.begin(), users_.end(),
                            [&](const UserModel& u) { return u.email == dto.email; });
            if(exists)
            {
                throw ValidationException("Email already in use");
            }

            UserModel user{
                .id            = next_id_++,
                .tenant_id     = dto.tenant_id.empty() ? "default" : dto.tenant_id,
                .username      = dto.username,
                .email         = dto.email,
                .password_hash = dto.password_hash,
                .role          = dto.role.empty() ? "user" : dto.role,
                .active        = dto.active,
            };

            users_.push_back(user);
            return user;
        }

        // Check email uniqueness
        if(repository_->email_exists(dto.email))
        {
            throw ValidationException("Email already in use");
        }

        // Check username uniqueness within tenant
        std::string tenant = dto.tenant_id.empty() ? "default" : dto.tenant_id;
        if(repository_->username_exists(tenant, dto.username))
        {
            throw ValidationException("Username already exists in tenant");
        }

        UserModel user{
            .id            = 0,
            .tenant_id     = tenant,
            .username      = dto.username,
            .email         = dto.email,
            .password_hash = dto.password_hash,
            .role          = dto.role.empty() ? "user" : dto.role,
            .active        = dto.active,
        };

        auto id = repository_->insert(user);
        user.id = id;
        return user;
    }

    UserModel UserService::update_user(std::int64_t id, const UpdateUserDto& dto)
    {
        if(use_memory_)
        {
            auto* user = find_user_by_id(id);
            if(!user)
            {
                throw NotFoundException("User not found");
            }

            if(dto.username.has_value())
            {
                user->username = dto.username.value();
            }
            if(dto.email.has_value())
            {
                auto exists = std::any_of(users_.begin(), users_.end(),
                                          [&](const UserModel& u) {
                                              return u.email == dto.email.value() &&
                                                     u.id != id;
                                          });
                if(exists)
                {
                    throw ValidationException("Email already in use");
                }
                user->email = dto.email.value();
            }
            if(dto.role.has_value())
            {
                user->role = dto.role.value();
            }
            if(dto.active.has_value())
            {
                user->active = dto.active.value();
            }

            return *user;
        }

        auto existing = repository_->find_by_id(id);
        if(!existing)
        {
            throw NotFoundException("User not found");
        }

        auto user = existing.value();

        if(dto.username.has_value())
        {
            user.username = dto.username.value();
        }
        if(dto.email.has_value())
        {
            // Check email uniqueness
            auto other = repository_->find_by_email(dto.email.value());
            if(other && other->id != id)
            {
                throw ValidationException("Email already in use");
            }
            user.email = dto.email.value();
        }
        if(dto.role.has_value())
        {
            user.role = dto.role.value();
        }
        if(dto.active.has_value())
        {
            user.active = dto.active.value();
        }

        repository_->update(user);
        return user;
    }

    void UserService::delete_user(std::int64_t id)
    {
        if(use_memory_)
        {
            auto* user = find_user_by_id(id);
            if(!user)
            {
                throw NotFoundException("User not found");
            }
            user->active = false;
            return;
        }

        auto existing = repository_->find_by_id(id);
        if(!existing)
        {
            throw NotFoundException("User not found");
        }

        // Soft delete by deactivating
        auto user  = existing.value();
        user.active = false;
        repository_->update(user);
    }

} // namespace services
