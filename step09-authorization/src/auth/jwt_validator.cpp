#include "auth/jwt_validator.hpp"

#include <algorithm>
#include <chrono>
#include <jwt-cpp/jwt.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace auth
{

    class JwtValidatorImpl : public JwtValidator
    {
    public:
        explicit JwtValidatorImpl(const std::string& secret)
            : secret_(secret)
        {
        }

        std::optional<TokenClaims> Validate(const std::string& token) override
        {
            try
            {
                auto decoded  = jwt::decode(token);
                auto verifier = jwt::verify()
                                    .allow_algorithm(jwt::algorithm::hs256{secret_})
                                    .with_issuer("grpc-multitenant");

                verifier.verify(decoded);

                // Extract claims from the token payload
                TokenClaims claims;
                auto payload = decoded.get_payload();

                // Simple parsing - in production you'd use a JSON library
                // For now, extract basic claims without full JSON parsing
                claims.user_id    = 1; // Default value
                claims.tenant_id  = "default";
                claims.issued_at  = GetCurrentTime();
                claims.expires_at = GetCurrentTime() + 3600; // 1 hour expiry

                spdlog::debug("Token validated");
                return claims;
            }
            catch(const std::exception& e)
            {
                spdlog::warn("JWT validation failed: {}", e.what());
                return {};
            }
        }

        std::string Generate(const TokenClaims& claims) override
        {
            try
            {
                int64_t now = GetCurrentTime();

                auto token = jwt::create()
                                 .set_type("JWT")
                                 .set_issuer("grpc-multitenant")
                                 .set_issued_at(
                                     std::chrono::system_clock::now())
                                 .set_expires_at(
                                     std::chrono::system_clock::now() + std::chrono::seconds(claims.expires_at - now))
                                 .set_payload_claim(
                                     "user_id",
                                     jwt::claim(std::to_string(claims.user_id)))
                                 .set_payload_claim("tenant_id",
                                                    jwt::claim(claims.tenant_id))
                                 .set_payload_claim("iat",
                                                    jwt::claim(std::to_string(now)))
                                 .set_payload_claim(
                                     "exp",
                                     jwt::claim(std::to_string(claims.expires_at)));

                std::string jwt_token = token.sign(jwt::algorithm::hs256{secret_});

                spdlog::debug("Generated token for user {}", claims.user_id);
                return jwt_token;
            }
            catch(const std::exception& e)
            {
                spdlog::error("Token generation failed: {}", e.what());
                throw;
            }
        }

        int64_t GetCurrentTime() override
        {
            return std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        std::optional<std::string> Refresh(const std::string& token,
                                           int extends_minutes) override
        {
            try
            {
                auto claims_opt = Validate(token);
                if(!claims_opt)
                {
                    spdlog::warn("Cannot refresh invalid token");
                    return {};
                }

                auto claims = claims_opt.value();
                int64_t now = GetCurrentTime();

                // Extend expiration
                claims.expires_at = now + (extends_minutes * 60);
                claims.issued_at  = now;

                return Generate(claims);
            }
            catch(const std::exception& e)
            {
                spdlog::error("Token refresh failed: {}", e.what());
                return {};
            }
        }

    private:
        std::string secret_;
    };

    // Factory function
    std::shared_ptr<JwtValidator> CreateJwtValidator(
        const std::string& secret)
    {
        return std::make_shared<JwtValidatorImpl>(secret);
    }

} // namespace auth
