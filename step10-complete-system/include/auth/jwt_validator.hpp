#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace auth
{

    /**
     * @brief JWT token claims
     */
    struct TokenClaims
    {
        int64_t user_id = 0;
        std::string tenant_id;
        std::vector<std::string> roles;
        int64_t issued_at  = 0;
        int64_t expires_at = 0;

        /**
         * @brief Check if token is expired
         */
        bool IsExpired(int64_t now_seconds) const
        {
            return now_seconds >= expires_at;
        }
    };

    /**
     * @brief JWT token validation and generation
     * Uses HS256 algorithm (HMAC with SHA-256)
     */
    class JwtValidator
    {
    public:
        virtual ~JwtValidator() = default;

        /**
         * @brief Validate and parse JWT token
         * @param token JWT token string
         * @return Parsed claims if valid, empty optional if invalid
         */
        virtual std::optional<TokenClaims> Validate(const std::string& token) = 0;

        /**
         * @brief Generate JWT token from claims
         * @param claims Token claims to encode
         * @return JWT token string
         */
        virtual std::string Generate(const TokenClaims& claims) = 0;

        /**
         * @brief Get current time in seconds since epoch
         */
        virtual int64_t GetCurrentTime() = 0;

        /**
         * @brief Refresh token with new expiration
         * @param token Current JWT token
         * @param extends_minutes How many minutes to extend
         * @return New JWT token, empty if original invalid
         */
        virtual std::optional<std::string> Refresh(
            const std::string& token, int extends_minutes = 60) = 0;
    };

    /**
     * @brief Factory function to create JWT validator
     */
    std::shared_ptr<JwtValidator> CreateJwtValidator(
        const std::string& secret);

} // namespace auth
