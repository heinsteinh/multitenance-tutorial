#include "services/user_service.hpp"

#include <algorithm>

namespace services {

UserService::UserService() : next_id_(1) {
  // seed with a default user for demo purposes
  UserModel admin{.id = next_id_++,
                  .username = "admin",
                  .email = "admin@example.com",
                  .role = "admin",
                  .active = true};
  users_.push_back(admin);
}

UserModel UserService::get_user(std::int64_t id) const {
  auto it = std::find_if(users_.begin(), users_.end(),
                         [&](const UserModel &u) { return u.id == id; });
  if (it == users_.end()) {
    throw NotFoundException("User not found");
  }
  return *it;
}

std::vector<UserModel> UserService::list_users() const { return users_; }

UserModel UserService::create_user(const CreateUserDto &dto) {
  if (dto.username.empty()) {
    throw ValidationException("Username is required");
  }
  if (dto.email.empty()) {
    throw ValidationException("Email is required");
  }

  auto exists =
      std::any_of(users_.begin(), users_.end(),
                  [&](const UserModel &u) { return u.email == dto.email; });
  if (exists) {
    throw ValidationException("Email already in use");
  }

  UserModel user{
      .id = next_id_++,
      .username = dto.username,
      .email = dto.email,
      .role = dto.role.empty() ? "user" : dto.role,
      .active = dto.active,
  };

  users_.push_back(user);
  return user;
}

UserModel UserService::get_user_by_username(const std::string &username) const {
  auto it = std::find_if(users_.begin(), users_.end(), [&](const UserModel &u) {
    return u.username == username;
  });
  if (it == users_.end()) {
    throw NotFoundException("User not found");
  }
  return *it;
}

UserModel UserService::update_user(std::int64_t id, const UpdateUserDto &dto) {
  auto it = std::find_if(users_.begin(), users_.end(),
                         [&](const UserModel &u) { return u.id == id; });
  if (it == users_.end()) {
    throw NotFoundException("User not found");
  }

  // Update fields if provided
  if (dto.username.has_value()) {
    it->username = dto.username.value();
  }
  if (dto.email.has_value()) {
    // Check email uniqueness
    auto exists =
        std::any_of(users_.begin(), users_.end(), [&](const UserModel &u) {
          return u.email == dto.email.value() && u.id != id;
        });
    if (exists) {
      throw ValidationException("Email already in use");
    }
    it->email = dto.email.value();
  }
  if (dto.role.has_value()) {
    it->role = dto.role.value();
  }
  if (dto.active.has_value()) {
    it->active = dto.active.value();
  }

  return *it;
}

void UserService::delete_user(std::int64_t id) {
  auto it = std::find_if(users_.begin(), users_.end(),
                         [&](const UserModel &u) { return u.id == id; });
  if (it == users_.end()) {
    throw NotFoundException("User not found");
  }

  // Soft delete - just deactivate the user
  it->active = false;
}

} // namespace services
