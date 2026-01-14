#pragma once

#include "dto.hpp"
#include "exceptions.hpp"

#include <vector>

namespace services {

class UserService {
public:
  UserService();

  UserModel get_user(std::int64_t id) const;
  UserModel get_user_by_username(const std::string &username) const;
  std::vector<UserModel> list_users() const;
  UserModel create_user(const CreateUserDto &dto);
  UserModel update_user(std::int64_t id, const UpdateUserDto &dto);
  void delete_user(std::int64_t id);

private:
  std::int64_t next_id_;
  std::vector<UserModel> users_;
};

} // namespace services
