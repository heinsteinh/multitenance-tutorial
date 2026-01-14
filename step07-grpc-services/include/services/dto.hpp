#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace services {

struct UserModel {
  std::int64_t id{0};
  std::string username;
  std::string email;
  std::string role;
  bool active{true};
};

struct TenantModel {
  std::int64_t id{0};
  std::string tenant_id;
  std::string name;
  std::string plan;
  bool active{true};
};

struct CreateUserDto {
  std::string username;
  std::string email;
  std::string password;
  std::string role;
  bool active{true};
};

struct UpdateUserDto {
  std::optional<std::string> username;
  std::optional<std::string> email;
  std::optional<std::string> password;
  std::optional<std::string> role;
  std::optional<bool> active;
};

struct CreateTenantDto {
  std::string tenant_id;
  std::string name;
  std::string plan;
  bool active{true};
};

struct UpdateTenantDto {
  std::optional<std::string> name;
  std::optional<std::string> plan;
  std::optional<bool> active;
};

} // namespace services
