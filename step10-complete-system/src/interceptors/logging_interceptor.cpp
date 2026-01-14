#include "interceptors/logging_interceptor.hpp"
#include <spdlog/spdlog.h>

namespace multitenant {

void LoggingInterceptor::Intercept(
    grpc::experimental::InterceptorBatchMethods *methods) {
  // Hook point: Receiving initial metadata (start of request)
  if (methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::
              POST_RECV_INITIAL_METADATA)) {
    method_name_ = get_method_name(methods);
    start_time_ = Clock::now();

    auto *metadata = methods->GetRecvInitialMetadata();
    auto request_id = get_metadata(metadata, "x-request-id");

    spdlog::debug("→ Request started: {} [request_id: {}]", method_name_,
                  request_id.value_or("none"));
  }

  // Hook point: Sending status (end of request)
  if (methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::PRE_SEND_STATUS)) {
    auto end_time = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time_);

    auto status = methods->GetSendStatus();

    if (status.ok()) {
      spdlog::info("← {} completed successfully in {}ms", method_name_,
                   duration.count());
    } else {
      spdlog::warn("← {} failed with {} ({}) in {}ms", method_name_,
                   static_cast<int>(status.error_code()),
                   status.error_message(), duration.count());
    }
  }

  methods->Proceed();
}

} // namespace multitenant
