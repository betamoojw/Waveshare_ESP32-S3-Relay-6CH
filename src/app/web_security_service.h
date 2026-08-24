#pragma once

#include "../ports/clock_port.h"
#include "../ports/web_crypto_port.h"
#include "../ports/web_security_port.h"
#include "../ports/web_security_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::app
{
enum class WebSecurityInitializeResult : std::uint8_t
{
	Initialized,
	InvalidDependency,
	NotProvisioned,
	InvalidRecord
};

enum class WebSessionResult : std::uint8_t
{
	Applied,
	InvalidCredentials,
	RateLimited,
	CapacityFull,
	Unauthorized,
	InvalidRequest,
	CryptoFailure
};

struct WebSessionView final
{
	std::uint32_t userId{0};
	std::array<char, ports::webUsernameCapacity> username{};
	ports::WebUserRole role{ports::WebUserRole::Guest};
	std::uint32_t permissions{0};
	std::uint32_t expiresInMs{0};
	std::array<char, 33> csrfToken{};
};

struct WebSessionCreated final
{
	WebSessionView view{};
	std::array<char, 768> jwt{};
};

struct WebUserView final
{
	std::uint32_t id{0};
	std::array<char, ports::webUsernameCapacity> username{};
	ports::WebUserRole role{ports::WebUserRole::Guest};
	bool enabled{false};
};

enum class WebUserManagementResult : std::uint8_t
{
	Applied,
	Invalid,
	DuplicateUsername,
	CapacityFull,
	LastAdministrator,
	PersistenceFailure,
	CryptoFailure
};

class WebSecurityService final
{
public:
	WebSecurityService(ports::WebSecurityStore store,
		ports::WebCryptoPort crypto,
		ports::ClockPort clock) noexcept;

	[[nodiscard]] WebSecurityInitializeResult initialize(std::string_view expectedOrigin,
		std::string_view expectedHost) noexcept;
	[[nodiscard]] WebSessionResult createSession(std::string_view username,
		std::string_view password,
		WebSessionCreated &created) noexcept;
	[[nodiscard]] WebSessionResult inspectSession(std::string_view jwt, WebSessionView &view) noexcept;
	[[nodiscard]] WebSessionResult deleteSession(std::string_view jwt,
		std::string_view origin,
		std::string_view host,
		std::string_view csrfToken) noexcept;
	[[nodiscard]] std::size_t users(std::array<WebUserView, ports::webUserCapacity> &users) const noexcept;
	[[nodiscard]] WebUserManagementResult saveUser(std::uint32_t id,
		std::string_view username,
		ports::WebUserRole role,
		bool enabled,
		std::string_view password,
		bool replacePassword) noexcept;
	[[nodiscard]] WebUserManagementResult provisionInitialAdministrator(std::string_view username,
		std::string_view password,
		std::string_view hostName) noexcept;
	[[nodiscard]] ports::WebSecurityStoreResult erase() noexcept;
	[[nodiscard]] ports::WebSecurityPort port() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	static constexpr std::uint32_t accessLifetimeMs{15U * 60U * 1000U};
	static constexpr std::uint32_t absoluteLifetimeMs{8U * 60U * 60U * 1000U};
	static constexpr std::uint32_t loginWindowMs{5U * 60U * 1000U};
	static constexpr std::uint8_t maximumLoginFailures{5};
	static constexpr std::uint32_t mutationWindowMs{1000};

	struct Session final
	{
		bool active{false};
		std::uint32_t id{0};
		std::uint32_t userId{0};
		std::uint32_t permissions{0};
		std::uint32_t issuedAtMs{0};
		std::uint32_t expiresAtMs{0};
		std::uint32_t absoluteExpiresAtMs{0};
		ports::WebUserRole role{ports::WebUserRole::Guest};
		std::array<char, ports::webUsernameCapacity> username{};
		std::array<char, 33> csrfToken{};
		std::array<char, 768> jwt{};
		std::uint32_t mutationWindowStartedAtMs{0};
		std::uint8_t relayMutations{0};
		std::uint8_t configurationMutations{0};
	};

	[[nodiscard]] static std::string_view certificateHandler(void *context) noexcept;
	[[nodiscard]] static std::string_view privateKeyHandler(void *context) noexcept;
	[[nodiscard]] static bool authorizeHandler(void *context,
		std::string_view sessionToken,
		std::string_view origin,
		std::string_view host,
		std::string_view csrfToken,
		ports::WebPermission permission,
		bool mutation,
		ports::WebAuthorization &authorization) noexcept;
	[[nodiscard]] bool authorize(std::string_view sessionToken,
		std::string_view origin,
		std::string_view host,
		std::string_view csrfToken,
		ports::WebPermission permission,
		bool mutation,
		ports::WebAuthorization &authorization) noexcept;
	[[nodiscard]] Session *findSession(std::string_view jwt) noexcept;
	[[nodiscard]] bool validateSession(Session &session, std::string_view jwt, std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool createJwt(const ports::WebUserRecord &user, Session &session) noexcept;
	[[nodiscard]] bool verifyJwt(std::string_view jwt) const noexcept;
	[[nodiscard]] bool mutationAllowed(Session &session, ports::WebPermission permission, std::uint32_t nowMs) noexcept;
	[[nodiscard]] static std::uint32_t permissionsFor(ports::WebUserRole role) noexcept;
	[[nodiscard]] static bool constantTimeEqual(std::string_view left, std::string_view right) noexcept;
	[[nodiscard]] static bool copyText(std::string_view input, char *output, std::size_t capacity) noexcept;
	[[nodiscard]] static std::size_t base64UrlEncode(const std::uint8_t *input,
		std::size_t inputSize,
		char *output,
		std::size_t outputCapacity) noexcept;
	[[nodiscard]] static bool recordIsValid(const ports::WebSecurityRecord &record) noexcept;
	void fillView(const Session &session, std::uint32_t nowMs, WebSessionView &view) const noexcept;
	void revokeUserSessions(std::uint32_t userId) noexcept;

	ports::WebSecurityStore store_;
	ports::WebCryptoPort crypto_;
	ports::ClockPort clock_;
	ports::WebSecurityRecord record_{};
	std::array<Session, ports::webSessionCapacity> sessions_{};
	std::array<char, 192> expectedOrigin_{};
	std::array<char, 96> expectedHost_{};
	std::uint32_t loginWindowStartedAtMs_{0};
	std::uint8_t loginFailures_{0};
	bool initialized_{false};
};
}