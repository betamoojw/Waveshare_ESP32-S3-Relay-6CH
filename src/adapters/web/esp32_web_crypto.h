#pragma once

#include "../../ports/web_crypto_port.h"

namespace switch_actuator::adapters::web
{
class Esp32WebCrypto final
{
public:
	[[nodiscard]] ports::WebCryptoPort port() noexcept;

private:
	[[nodiscard]] static bool random(void *context, std::uint8_t *output, std::size_t size) noexcept;
	[[nodiscard]] static bool hmacSha256(void *context,
		const std::uint8_t *key,
		std::size_t keySize,
		std::string_view message,
		std::uint8_t *output,
		std::size_t outputSize) noexcept;
	[[nodiscard]] static bool verifyPassword(void *context,
		std::string_view password,
		const std::uint8_t *salt,
		std::size_t saltSize,
		std::uint32_t iterations,
		const std::uint8_t *expected,
		std::size_t expectedSize) noexcept;
	[[nodiscard]] static bool derivePassword(void *context,
		std::string_view password,
		const std::uint8_t *salt,
		std::size_t saltSize,
		std::uint32_t iterations,
		std::uint8_t *output,
		std::size_t outputSize) noexcept;
	[[nodiscard]] static bool generateIdentity(void *context,
		std::string_view hostName,
		char *certificate,
		std::size_t certificateCapacity,
		char *privateKey,
		std::size_t privateKeyCapacity) noexcept;
	[[nodiscard]] static bool certificateFingerprint(void *context,
		std::string_view certificate,
		std::uint8_t *output,
		std::size_t outputSize) noexcept;
};
}