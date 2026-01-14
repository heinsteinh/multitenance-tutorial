#include "common.pb.h"
#include "tenant.pb.h"
#include "user.pb.h"
#include <catch2/catch_test_macros.hpp>

using namespace multitenant::v1;

TEST_CASE("User protobuf message", "[grpc]") {
  User user;
  user.set_id(1);
  user.set_username("alice");
  user.set_email("alice@example.com");
  user.set_role("admin");
  user.set_active(true);

  REQUIRE(user.id() == 1);
  REQUIRE(user.username() == "alice");
  REQUIRE(user.email() == "alice@example.com");
  REQUIRE(user.role() == "admin");
  REQUIRE(user.active() == true);
}

TEST_CASE("User serialization", "[grpc]") {
  User original;
  original.set_id(42);
  original.set_username("bob");
  original.set_email("bob@example.com");
  original.set_role("user");

  std::string serialized = original.SerializeAsString();
  REQUIRE(serialized.length() > 0);
  REQUIRE(serialized.length() < 200);

  User deserialized;
  REQUIRE(deserialized.ParseFromString(serialized));
  REQUIRE(deserialized.id() == 42);
  REQUIRE(deserialized.username() == "bob");
}

TEST_CASE("ListUsersResponse", "[grpc]") {
  ListUsersResponse response;

  for (int i = 0; i < 3; ++i) {
    auto *user = response.add_users();
    user->set_id(i);
    user->set_username("user" + std::to_string(i));
    user->set_email("user" + std::to_string(i) + "@example.com");
  }

  REQUIRE(response.users_size() == 3);
  REQUIRE(response.users(0).username() == "user0");
  REQUIRE(response.users(2).id() == 2);
}

TEST_CASE("CreateUserRequest", "[grpc]") {
  CreateUserRequest request;
  request.set_username("charlie");
  request.set_email("charlie@example.com");
  request.set_password("secret");
  request.set_role("user");

  REQUIRE(request.username() == "charlie");
  REQUIRE(request.email() == "charlie@example.com");
  REQUIRE(request.role() == "user");
}

TEST_CASE("Permission message", "[grpc]") {
  Permission perm;
  perm.set_id(1);
  perm.set_user_id(10);
  perm.set_resource("users");
  perm.set_action("read");
  perm.set_allowed(true);

  REQUIRE(perm.user_id() == 10);
  REQUIRE(perm.resource() == "users");
  REQUIRE(perm.action() == "read");
  REQUIRE(perm.allowed() == true);
}

TEST_CASE("Empty message", "[grpc]") {
  Empty empty;
  std::string serialized = empty.SerializeAsString();
  REQUIRE(serialized.length() <= 1);
}

TEST_CASE("PaginationInfo", "[grpc]") {
  PaginationInfo info;
  info.set_page(1);
  info.set_page_size(10);
  info.set_total_pages(5);
  info.set_total_items(50);

  REQUIRE(info.page() == 1);
  REQUIRE(info.total_items() == 50);
}

TEST_CASE("GetUserResponse", "[grpc]") {
  GetUserResponse response;
  auto *user = response.mutable_user();
  user->set_id(100);
  user->set_username("alice");
  user->set_email("alice@corp.com");

  REQUIRE(response.user().id() == 100);
  REQUIRE(response.user().username() == "alice");
}
