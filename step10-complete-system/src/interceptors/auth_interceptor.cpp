#include "interceptors/auth_interceptor.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace multitenant {

void AuthInterceptor::Intercept(
    grpc::experimental::InterceptorBatchMethods *methods) {
  if (methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::
              PRE_RECV_INITIAL_METADATA)) {

    auto *metadata = methods->GetRecvInitialMetadata();

    // For now, just log and proceed
    auto auth_header = get_metadata(metadata, "authorization");
    if (auth_header.has_value()) {
      spdlog::debug("Authorization header present");
    }
  }

  methods->Proceed();
}

std::optional<std::string>
AuthInterceptor::extract_bearer_token(const std::string &auth_header) {
  // Expected format: "Bearer <token>"
  const std::string prefix = "Bearer ";

  if (auth_header.size() <= prefix.size()) {
    return std::nullopt;
  }

  if (auth_header.substr(0, prefix.size()) != prefix) {
    return std::nullopt;
  }

  return auth_header.substr(prefix.size());
}

std::optional<std::string>
AuthInterceptor::validate_token(const std::string &token) {
  // Simple validation - in real app would validate JWT signature
  // For demo, accept any non-empty token and extract user id
  if (token.empty()) {
    return std::nullopt;
  }

  // In real app, would decode JWT and extract user_id from claims
  // For demo, just return "user-1"
  return "user-from-token";
}

} // namespace multitenant
