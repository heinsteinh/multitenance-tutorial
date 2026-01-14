#include "interceptors/interceptor_factory.hpp"
#include <spdlog/spdlog.h>

namespace multitenant {

// Helper class to chain multiple interceptors
class ChainedInterceptor : public grpc::experimental::Interceptor {
public:
  explicit ChainedInterceptor(
      std::vector<std::unique_ptr<grpc::experimental::Interceptor>>
          interceptors)
      : interceptors_(std::move(interceptors)), current_index_(0) {}

  void
  Intercept(grpc::experimental::InterceptorBatchMethods *methods) override {
    if (current_index_ < interceptors_.size()) {
      interceptors_[current_index_++]->Intercept(methods);
    } else {
      methods->Proceed();
    }
  }

private:
  std::vector<std::unique_ptr<grpc::experimental::Interceptor>> interceptors_;
  size_t current_index_;
};

grpc::experimental::Interceptor *InterceptorFactory::CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo *info) {

  // Create interceptor chain: Logging -> Auth -> Tenant
  std::vector<std::unique_ptr<grpc::experimental::Interceptor>> interceptors;
  interceptors.push_back(std::make_unique<LoggingInterceptor>());
  interceptors.push_back(std::make_unique<AuthInterceptor>());
  interceptors.push_back(std::make_unique<TenantInterceptor>());

  return new ChainedInterceptor(std::move(interceptors));
}

} // namespace multitenant
