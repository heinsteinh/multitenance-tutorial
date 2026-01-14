#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_interceptor.h>
#include <optional>
#include <string>

namespace multitenant {

// Helper to extract metadata from gRPC context
std::optional<std::string>
get_metadata(const std::multimap<grpc::string_ref, grpc::string_ref> *metadata,
             const std::string &key);

// Base class for server interceptors
class BaseServerInterceptor : public grpc::experimental::Interceptor {
public:
  BaseServerInterceptor() = default;
  virtual ~BaseServerInterceptor() = default;

protected:
  // Helper to get method name from interceptor batch methods
  std::string
  get_method_name(grpc::experimental::InterceptorBatchMethods *methods);

  // Helper to check if method requires authentication
  bool is_protected_method(const std::string &method_name);
};

} // namespace multitenant
