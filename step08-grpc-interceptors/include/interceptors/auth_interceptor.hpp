#pragma once

#include "interceptors/base_interceptor.hpp"
#include <optional>
#include <string>

namespace multitenant {

// Simple authentication interceptor that validates bearer tokens
class AuthInterceptor : public BaseServerInterceptor {
public:
  AuthInterceptor() = default;

  void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override;

private:
  // Simple token validation (in real app, would validate JWT)
  std::optional<std::string> validate_token(const std::string &token);

  // Extract bearer token from authorization header
  std::optional<std::string>
  extract_bearer_token(const std::string &auth_header);
};

} // namespace multitenant
