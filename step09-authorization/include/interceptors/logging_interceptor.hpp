#pragma once

#include "interceptors/base_interceptor.hpp"
#include <chrono>
#include <string>

namespace multitenant {

// Interceptor for logging requests and responses
class LoggingInterceptor : public BaseServerInterceptor {
public:
  LoggingInterceptor() = default;

  void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override;

private:
  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  TimePoint start_time_;
  std::string method_name_;
};

} // namespace multitenant
