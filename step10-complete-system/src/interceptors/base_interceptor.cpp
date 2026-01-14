#include "interceptors/base_interceptor.hpp"
#include <spdlog/spdlog.h>

namespace multitenant {

std::optional<std::string>
get_metadata(const std::multimap<grpc::string_ref, grpc::string_ref> *metadata,
             const std::string &key) {

  if (!metadata) {
    return std::nullopt;
  }

  for (const auto &[k, v] : *metadata) {
    if (std::string(k.data(), k.size()) == key) {
      return std::string(v.data(), v.size());
    }
  }

  return std::nullopt;
}

std::string BaseServerInterceptor::get_method_name(
    grpc::experimental::InterceptorBatchMethods *methods) {
  // Note: Method name is not directly available in InterceptorBatchMethods
  // This is typically handled through ServerRpcInfo in the factory
  // For now, return a placeholder
  return "unknown-method";
}

bool BaseServerInterceptor::is_protected_method(
    const std::string &method_name) {
  // Public methods that don't require authentication
  static const std::vector<std::string> public_methods = {
      "/multitenant.v1.UserService/CreateUser",
      // Add other public methods here
  };

  for (const auto &public_method : public_methods) {
    if (method_name == public_method) {
      return false;
    }
  }

  return true;
}

} // namespace multitenant
