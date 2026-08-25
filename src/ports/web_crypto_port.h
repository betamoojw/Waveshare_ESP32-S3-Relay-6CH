#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::ports
{
using WebRandomHandler = bool (*)(void *context, std::uint8_t *output, std::size_t size) noexcept;
using WebHmacSha256Handler = bool (*)(void *context,
	const std::uint8_t *key,
	std::size_t keySize,
	std::string_view message,
	std::uint8_t *output,
	std::size_t outputSize) noexcept;
using WebPasswordVerifyHandler = bool (*)(void *context,
	std::string_view password,
	const std::uint8_t *salt,
	std::size_t saltSize,
	std::uint32_t iterations,
	const std::uint8_t *expected,
	std::size_t expectedSize) noexcept;
using WebPasswordDeriveHandler = bool (*)(void *context,
	std::string_view password,
	const std::uint8_t *salt,
	std::size_t saltSize,
	std::uint32_t iterations,
	std::uint8_t *output,
	std::size_t outputSize) noexcept;
using WebIdentityGenerateHandler = bool (*)(void *context,
	std::string_view hostName,
	char *certificate,
	std::size_t certificateCapacity,
	char *privateKey,
	std::size_t privateKeyCapacity) noexcept;
using WebCertificateFingerprintHandler = bool (*)(void *context,
	std::string_view certificate,
	std::uint8_t *output,
	std::size_t outputSize) noexcept;

class WebCryptoPort final
{
public:
	constexpr WebCryptoPort() noexcept = default;
	constexpr WebCryptoPort(WebRandomHandler randomHandler,
		WebHmacSha256Handler hmacHandler,
		WebPasswordVerifyHandler passwordVerifyHandler,
		WebPasswordDeriveHandler passwordDeriveHandler,
		WebIdentityGenerateHandler identityGenerateHandler,
		WebCertificateFingerprintHandler certificateFingerprintHandler,
		void *context = nullptr) noexcept
		: randomHandler_{randomHandler}, hmacHandler_{hmacHandler}, passwordVerifyHandler_{passwordVerifyHandler},
		  passwordDeriveHandler_{passwordDeriveHandler}, identityGenerateHandler_{identityGenerateHandler},
		  certificateFingerprintHandler_{certificateFingerprintHandler}, context_{context}
	{
	}

	[[nodiscard]] bool random(std::uint8_t *output, const std::size_t size) const noexcept
	{
		return randomHandler_ != nullptr && randomHandler_(context_, output, size);
	}
	[[nodiscard]] bool hmacSha256(const std::uint8_t *key,
		const std::size_t keySize,
		const std::string_view message,
		std::uint8_t *output,
		const std::size_t outputSize) const noexcept
	{
		return hmacHandler_ != nullptr && hmacHandler_(context_, key, keySize, message, output, outputSize);
	}
	[[nodiscard]] bool verifyPassword(const std::string_view password,
		const std::uint8_t *salt,
		const std::size_t saltSize,
		const std::uint32_t iterations,
		const std::uint8_t *expected,
		const std::size_t expectedSize) const noexcept
	{
		return passwordVerifyHandler_ != nullptr && passwordVerifyHandler_(context_, password, salt, saltSize,
			iterations, expected, expectedSize);
	}
	[[nodiscard]] bool derivePassword(const std::string_view password,
		const std::uint8_t *salt,
		const std::size_t saltSize,
		const std::uint32_t iterations,
		std::uint8_t *output,
		const std::size_t outputSize) const noexcept
	{
		return passwordDeriveHandler_ != nullptr && passwordDeriveHandler_(context_, password, salt, saltSize,
			iterations, output, outputSize);
	}
	[[nodiscard]] bool generateIdentity(const std::string_view hostName,
		char *certificate,
		const std::size_t certificateCapacity,
		char *privateKey,
		const std::size_t privateKeyCapacity) const noexcept
	{
		return identityGenerateHandler_ != nullptr && identityGenerateHandler_(context_, hostName, certificate,
			certificateCapacity, privateKey, privateKeyCapacity);
	}
	[[nodiscard]] bool certificateFingerprint(const std::string_view certificate,
		std::uint8_t *output,
		const std::size_t outputSize) const noexcept
	{
		return certificateFingerprintHandler_ != nullptr &&
			certificateFingerprintHandler_(context_, certificate, output, outputSize);
	}
	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return randomHandler_ != nullptr && hmacHandler_ != nullptr && passwordVerifyHandler_ != nullptr &&
			passwordDeriveHandler_ != nullptr && identityGenerateHandler_ != nullptr &&
			certificateFingerprintHandler_ != nullptr;
	}

private:
	WebRandomHandler randomHandler_{nullptr};
	WebHmacSha256Handler hmacHandler_{nullptr};
	WebPasswordVerifyHandler passwordVerifyHandler_{nullptr};
	WebPasswordDeriveHandler passwordDeriveHandler_{nullptr};
	WebIdentityGenerateHandler identityGenerateHandler_{nullptr};
	WebCertificateFingerprintHandler certificateFingerprintHandler_{nullptr};
	void *context_{nullptr};
};
}