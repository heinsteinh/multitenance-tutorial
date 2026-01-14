#pragma once

#include <grpcpp/support/status.h>

#include <stdexcept>
#include <string>

namespace services {

class ServiceException : public std::runtime_error {
public:
  explicit ServiceException(const std::string &message)
      : std::runtime_error(message) {}
};

class NotFoundException : public ServiceException {
public:
  explicit NotFoundException(const std::string &message)
      : ServiceException(message) {}
};

class ValidationException : public ServiceException {
public:
  explicit ValidationException(const std::string &message)
      : ServiceException(message) {}
};

class AuthorizationException : public ServiceException {
public:
  explicit AuthorizationException(const std::string &message)
      : ServiceException(message) {}
};

inline grpc::Status map_exception_to_status(const std::exception &ex) {
  if (auto *not_found = dynamic_cast<const NotFoundException *>(&ex)) {
    return {grpc::StatusCode::NOT_FOUND, not_found->what()};
  }
  if (auto *validation = dynamic_cast<const ValidationException *>(&ex)) {
    return {grpc::StatusCode::INVALID_ARGUMENT, validation->what()};
  }
  if (auto *auth = dynamic_cast<const AuthorizationException *>(&ex)) {
    return {grpc::StatusCode::PERMISSION_DENIED, auth->what()};
  }
  if (dynamic_cast<const ServiceException *>(&ex) != nullptr) {
    return {grpc::StatusCode::FAILED_PRECONDITION, ex.what()};
  }
  return {grpc::StatusCode::INTERNAL, ex.what()};
}

} // namespace services
