#include "services/exceptions.hpp"
#include "services/mapper.hpp"
#include "services/user_service.hpp"

#include <grpcpp/grpcpp.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "user.grpc.pb.h"

namespace handlers {

class UserHandler : public multitenant::v1::UserService::Service {
public:
  explicit UserHandler(services::UserService &service) : service_(service) {}

  grpc::Status GetUser(grpc::ServerContext * /*context*/,
                       const multitenant::v1::GetUserRequest *request,
                       multitenant::v1::GetUserResponse *response) override {
    try {
      auto user = service_.get_user(request->user_id());
      *response->mutable_user() = services::mapper::to_proto(user);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  ListUsers(grpc::ServerContext * /*context*/,
            const multitenant::v1::ListUsersRequest * /*request*/,
            multitenant::v1::ListUsersResponse *response) override {
    try {
      auto users = service_.list_users();
      for (const auto &user : users) {
        *response->add_users() = services::mapper::to_proto(user);
      }
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  CreateUser(grpc::ServerContext * /*context*/,
             const multitenant::v1::CreateUserRequest *request,
             multitenant::v1::CreateUserResponse *response) override {
    try {
      auto dto = services::mapper::from_proto(*request);
      auto user = service_.create_user(dto);
      *response->mutable_user() = services::mapper::to_proto(user);
      spdlog::info("Created user '{}' ({})", user.username, user.email);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  GetUserByUsername(grpc::ServerContext * /*context*/,
                    const multitenant::v1::GetUserByUsernameRequest *request,
                    multitenant::v1::GetUserResponse *response) override {
    try {
      auto user = service_.get_user_by_username(request->username());
      *response->mutable_user() = services::mapper::to_proto(user);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  UpdateUser(grpc::ServerContext * /*context*/,
             const multitenant::v1::UpdateUserRequest *request,
             multitenant::v1::UpdateUserResponse *response) override {
    try {
      auto dto = services::mapper::from_proto(*request);
      auto user = service_.update_user(request->user_id(), dto);
      *response->mutable_user() = services::mapper::to_proto(user);
      spdlog::info("Updated user ID {}", user.id);
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

  grpc::Status
  DeleteUser(grpc::ServerContext * /*context*/,
             const multitenant::v1::DeleteUserRequest *request,
             multitenant::v1::DeleteUserResponse * /*response*/) override {
    try {
      service_.delete_user(request->user_id());
      spdlog::info("Deleted user ID {}", request->user_id());
      return grpc::Status::OK;
    } catch (const std::exception &ex) {
      return services::map_exception_to_status(ex);
    }
  }

private:
  services::UserService &service_;
};

} // namespace handlers

std::unique_ptr<multitenant::v1::UserService::Service>
make_user_handler(services::UserService &service) {
  return std::make_unique<handlers::UserHandler>(service);
}
