#pragma once

#include <cstdint>
#include <string_view>

namespace switch_actuator::ports
{
enum class WebPermission : std::uint32_t
{
	None = 0,
	RelayRead = 1U << 0U,
	RelayCommand = 1U << 1U,
	DiagnosticsRead = 1U << 2U,
	ConfigurationRead = 1U << 3U,
	ConfigurationWrite = 1U << 4U,
	UsersManage = 1U << 5U,
	FirmwareUpdate = 1U << 6U
};

struct WebAuthorization final
{
	std::uint32_t sessionId{0};
	std::uint32_t permissions{0};
};

enum class WebAuthorizationResult : std::uint8_t
{
	Authorized,
	Unauthenticated,
	Forbidden,
	RateLimited
};

using WebSecurityTextHandler = std::string_view (*)(void *context) noexcept;
using WebAuthorizeHandler = WebAuthorizationResult (*)(void *context,
	std::string_view sessionToken,
	std::string_view origin,
	std::string_view host,
	std::string_view csrfToken,
	WebPermission permission,
	bool mutation,
	WebAuthorization &authorization) noexcept;

class WebSecurityPort final
{
public:
	constexpr WebSecurityPort() noexcept = default;
	constexpr WebSecurityPort(WebSecurityTextHandler certificateHandler,
		WebSecurityTextHandler privateKeyHandler,
		WebAuthorizeHandler authorizeHandler,
		void *context = nullptr) noexcept
		: certificateHandler_{certificateHandler}, privateKeyHandler_{privateKeyHandler}, authorizeHandler_{authorizeHandler},
		  context_{context}
	{
	}

	[[nodiscard]] std::string_view certificate() const noexcept
	{
		return certificateHandler_ != nullptr ? certificateHandler_(context_) : std::string_view{};
	}
	[[nodiscard]] std::string_view privateKey() const noexcept
	{
		return privateKeyHandler_ != nullptr ? privateKeyHandler_(context_) : std::string_view{};
	}
	[[nodiscard]] WebAuthorizationResult authorize(std::string_view sessionToken,
		std::string_view origin,
		std::string_view host,
		std::string_view csrfToken,
		WebPermission permission,
		bool mutation,
		WebAuthorization &authorization) const noexcept
	{
		return authorizeHandler_ != nullptr
			? authorizeHandler_(context_, sessionToken, origin, host, csrfToken, permission, mutation, authorization)
			: WebAuthorizationResult::Unauthenticated;
	}
	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return certificateHandler_ != nullptr && privateKeyHandler_ != nullptr && authorizeHandler_ != nullptr;
	}

private:
	WebSecurityTextHandler certificateHandler_{nullptr};
	WebSecurityTextHandler privateKeyHandler_{nullptr};
	WebAuthorizeHandler authorizeHandler_{nullptr};
	void *context_{nullptr};
};
}