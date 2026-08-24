#include "web_security_service.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace switch_actuator::app
{
namespace
{
constexpr std::string_view jwtHeader{"{\"alg\":\"HS256\",\"typ\":\"JWT\"}"};
constexpr std::string_view jwtHeaderEncoded{"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"};
constexpr char hexDigits[]{"0123456789abcdef"};

void encodeHex(const std::uint8_t *input, const std::size_t size, char *output) noexcept
{
	for (std::size_t index = 0; index < size; ++index)
	{
		output[index * 2U] = hexDigits[input[index] >> 4U];
		output[index * 2U + 1U] = hexDigits[input[index] & 0x0FU];
	}
	output[size * 2U] = '\0';
}
}

WebSecurityService::WebSecurityService(const ports::WebSecurityStore store,
	const ports::WebCryptoPort crypto,
	const ports::ClockPort clock) noexcept
	: store_{store}, crypto_{crypto}, clock_{clock}
{
}

WebSecurityInitializeResult WebSecurityService::initialize(const std::string_view expectedOrigin,
	const std::string_view expectedHost) noexcept
{
	initialized_ = false;
	sessions_.fill({});
	if (!store_.isValid() || !crypto_.isValid() || !clock_.isValid() ||
		!copyText(expectedOrigin, expectedOrigin_.data(), expectedOrigin_.size()) ||
		!copyText(expectedHost, expectedHost_.data(), expectedHost_.size()))
	{
		return WebSecurityInitializeResult::InvalidDependency;
	}
	const auto loadResult = store_.load(record_);
	if (loadResult == ports::WebSecurityStoreResult::NotFound)
	{
		return WebSecurityInitializeResult::NotProvisioned;
	}
	if (loadResult != ports::WebSecurityStoreResult::Applied || !recordIsValid(record_))
	{
		return WebSecurityInitializeResult::InvalidRecord;
	}
	loginWindowStartedAtMs_ = clock_.nowMs();
	loginFailures_ = 0;
	initialized_ = true;
	return WebSecurityInitializeResult::Initialized;
}

WebSessionResult WebSecurityService::createSession(const std::string_view username,
	const std::string_view password,
	WebSessionCreated &created) noexcept
{
	created = {};
	if (!initialized_ || username.empty() || username.size() >= ports::webUsernameCapacity || password.empty() ||
		password.size() > 128U)
	{
		return WebSessionResult::InvalidRequest;
	}
	const auto nowMs = clock_.nowMs();
	if (nowMs - loginWindowStartedAtMs_ >= loginWindowMs)
	{
		loginWindowStartedAtMs_ = nowMs;
		loginFailures_ = 0;
	}
	if (loginFailures_ >= maximumLoginFailures)
	{
		return WebSessionResult::RateLimited;
	}
	const ports::WebUserRecord *matchingUser{nullptr};
	const ports::WebUserRecord *fallbackUser{nullptr};
	for (const auto &candidate : record_.users)
	{
		if (candidate.id == 0) continue;
		if (fallbackUser == nullptr) fallbackUser = &candidate;
		if (constantTimeEqual(username,
			std::string_view{candidate.username.data(), strnlen(candidate.username.data(), candidate.username.size())}))
		{
			matchingUser = &candidate;
		}
	}
	const auto *verificationUser = matchingUser != nullptr ? matchingUser : fallbackUser;
	const auto passwordValid = verificationUser != nullptr && crypto_.verifyPassword(password,
		verificationUser->passwordSalt.data(), verificationUser->passwordSalt.size(),
		verificationUser->passwordIterations, verificationUser->passwordVerifier.data(),
		verificationUser->passwordVerifier.size());
	if (matchingUser == nullptr || !matchingUser->enabled || !passwordValid)
	{
		++loginFailures_;
		return WebSessionResult::InvalidCredentials;
	}
	const auto user = matchingUser;
	const auto slot = std::find_if(sessions_.begin(), sessions_.end(), [](const auto &session) { return !session.active; });
	if (slot == sessions_.end())
	{
		return WebSessionResult::CapacityFull;
	}
	std::array<std::uint8_t, 20> random{};
	if (!crypto_.random(random.data(), random.size()))
	{
		return WebSessionResult::CryptoFailure;
	}
	*slot = {};
	std::memcpy(&slot->id, random.data(), sizeof(slot->id));
	if (slot->id == 0) slot->id = 1;
	slot->active = true;
	slot->userId = user->id;
	slot->role = user->role;
	slot->permissions = permissionsFor(user->role);
	slot->issuedAtMs = nowMs;
	slot->expiresAtMs = nowMs + accessLifetimeMs;
	slot->absoluteExpiresAtMs = nowMs + absoluteLifetimeMs;
	if (!copyText(std::string_view{user->username.data(), strnlen(user->username.data(), user->username.size())},
		slot->username.data(), slot->username.size()))
	{
		*slot = {};
		return WebSessionResult::InvalidRequest;
	}
	encodeHex(random.data() + sizeof(slot->id), 16, slot->csrfToken.data());
	if (!createJwt(*user, *slot))
	{
		*slot = {};
		return WebSessionResult::CryptoFailure;
	}
	loginFailures_ = 0;
	created.jwt = slot->jwt;
	fillView(*slot, nowMs, created.view);
	return WebSessionResult::Applied;
}

WebSessionResult WebSecurityService::inspectSession(const std::string_view jwt, WebSessionView &view) noexcept
{
	view = {};
	auto *session = findSession(jwt);
	const auto nowMs = clock_.nowMs();
	if (session == nullptr || !validateSession(*session, jwt, nowMs)) return WebSessionResult::Unauthorized;
	fillView(*session, nowMs, view);
	return WebSessionResult::Applied;
}

WebSessionResult WebSecurityService::deleteSession(const std::string_view jwt,
	const std::string_view origin,
	const std::string_view host,
	const std::string_view csrfToken) noexcept
{
	auto *session = findSession(jwt);
	const auto nowMs = clock_.nowMs();
	if (session == nullptr || !validateSession(*session, jwt, nowMs) ||
		!constantTimeEqual(origin, expectedOrigin_.data()) || !constantTimeEqual(host, expectedHost_.data()) ||
		!constantTimeEqual(csrfToken, session->csrfToken.data()))
	{
		return WebSessionResult::Unauthorized;
	}
	*session = {};
	return WebSessionResult::Applied;
}

std::size_t WebSecurityService::users(std::array<WebUserView, ports::webUserCapacity> &users) const noexcept
{
	users.fill({});
	std::size_t count{0};
	for (const auto &record : record_.users)
	{
		if (record.id == 0) continue;
		auto &view = users[count++];
		view.id = record.id;
		view.username = record.username;
		view.role = record.role;
		view.enabled = record.enabled;
	}
	return count;
}

WebUserManagementResult WebSecurityService::saveUser(const std::uint32_t id,
	const std::string_view username,
	const ports::WebUserRole role,
	const bool enabled,
	const std::string_view password,
	const bool replacePassword) noexcept
{
	if (!initialized_ || username.empty() || username.size() >= ports::webUsernameCapacity ||
		(replacePassword && (password.size() < 12U || password.size() > 128U)))
		return WebUserManagementResult::Invalid;
	auto replacement = record_;
	auto target = id == 0 ? std::find_if(replacement.users.begin(), replacement.users.end(),
		[](const auto &user) { return user.id == 0; }) :
		std::find_if(replacement.users.begin(), replacement.users.end(), [id](const auto &user) { return user.id == id; });
	if (target == replacement.users.end()) return id == 0 ? WebUserManagementResult::CapacityFull :
		WebUserManagementResult::Invalid;
	if (std::any_of(replacement.users.begin(), replacement.users.end(), [target, username](const auto &user) {
		return &user != &*target && user.id != 0 && constantTimeEqual(username,
			std::string_view{user.username.data(), strnlen(user.username.data(), user.username.size())});
	})) return WebUserManagementResult::DuplicateUsername;
	if (id == 0)
	{
		if (!replacePassword) return WebUserManagementResult::Invalid;
		std::uint32_t maximumId{0};
		for (const auto &user : replacement.users) maximumId = std::max(maximumId, user.id);
		target->id = maximumId + 1U;
		if (target->id == 0) return WebUserManagementResult::CapacityFull;
	}
	if (!copyText(username, target->username.data(), target->username.size())) return WebUserManagementResult::Invalid;
	target->role = role;
	target->enabled = enabled;
	if (replacePassword)
	{
		target->passwordIterations = 100'000U;
		if (!crypto_.random(target->passwordSalt.data(), target->passwordSalt.size()) ||
			!crypto_.derivePassword(password, target->passwordSalt.data(), target->passwordSalt.size(),
				target->passwordIterations, target->passwordVerifier.data(), target->passwordVerifier.size()))
			return WebUserManagementResult::CryptoFailure;
	}
	const auto hasAdministrator = std::any_of(replacement.users.begin(), replacement.users.end(), [](const auto &user) {
		return user.id != 0 && user.enabled && user.role == ports::WebUserRole::Administrator;
	});
	if (!hasAdministrator) return WebUserManagementResult::LastAdministrator;
	if (store_.save(replacement) != ports::WebSecurityStoreResult::Applied)
		return WebUserManagementResult::PersistenceFailure;
	record_ = replacement;
	revokeUserSessions(target->id);
	return WebUserManagementResult::Applied;
}

WebUserManagementResult WebSecurityService::provisionInitialAdministrator(const std::string_view username,
	const std::string_view password,
	const std::string_view hostName) noexcept
{
	if (!store_.isValid() || !crypto_.isValid() || username.empty() || username.size() >= ports::webUsernameCapacity ||
		password.size() < 12U || password.size() > 128U || hostName.empty() || hostName.size() >= expectedHost_.size())
		return WebUserManagementResult::Invalid;
	ports::WebSecurityRecord replacement{};
	if (!crypto_.random(replacement.signingKey.data(), replacement.signingKey.size()) ||
		!crypto_.generateIdentity(hostName, replacement.certificate.data(), replacement.certificate.size(),
			replacement.privateKey.data(), replacement.privateKey.size())) return WebUserManagementResult::CryptoFailure;
	auto &administrator = replacement.users[0];
	administrator.id = 1;
	administrator.role = ports::WebUserRole::Administrator;
	administrator.enabled = true;
	administrator.passwordIterations = 100'000U;
	if (!copyText(username, administrator.username.data(), administrator.username.size()) ||
		!crypto_.random(administrator.passwordSalt.data(), administrator.passwordSalt.size()) ||
		!crypto_.derivePassword(password, administrator.passwordSalt.data(), administrator.passwordSalt.size(),
			administrator.passwordIterations, administrator.passwordVerifier.data(), administrator.passwordVerifier.size()))
		return WebUserManagementResult::CryptoFailure;
	if (!recordIsValid(replacement)) return WebUserManagementResult::CryptoFailure;
	if (store_.save(replacement) != ports::WebSecurityStoreResult::Applied)
		return WebUserManagementResult::PersistenceFailure;
	record_ = replacement;
	sessions_.fill({});
	initialized_ = true;
	return WebUserManagementResult::Applied;
}

ports::WebSecurityStoreResult WebSecurityService::erase() noexcept
{
	initialized_ = false;
	sessions_.fill({});
	record_ = {};
	return store_.erase();
}

ports::WebSecurityPort WebSecurityService::port() noexcept
{
	return {certificateHandler, privateKeyHandler, authorizeHandler, this};
}

bool WebSecurityService::isInitialized() const noexcept { return initialized_; }

std::string_view WebSecurityService::certificateHandler(void *const context) noexcept
{
	const auto &service = *static_cast<WebSecurityService *>(context);
	return {service.record_.certificate.data(), strnlen(service.record_.certificate.data(), service.record_.certificate.size()) + 1U};
}

std::string_view WebSecurityService::privateKeyHandler(void *const context) noexcept
{
	const auto &service = *static_cast<WebSecurityService *>(context);
	return {service.record_.privateKey.data(), strnlen(service.record_.privateKey.data(), service.record_.privateKey.size()) + 1U};
}

bool WebSecurityService::authorizeHandler(void *const context,
	const std::string_view sessionToken,
	const std::string_view origin,
	const std::string_view host,
	const std::string_view csrfToken,
	const ports::WebPermission permission,
	const bool mutation,
	ports::WebAuthorization &authorization) noexcept
{
	return static_cast<WebSecurityService *>(context)->authorize(sessionToken, origin, host, csrfToken,
		permission, mutation, authorization);
}

bool WebSecurityService::authorize(const std::string_view sessionToken,
	const std::string_view origin,
	const std::string_view host,
	const std::string_view csrfToken,
	const ports::WebPermission permission,
	const bool mutation,
	ports::WebAuthorization &authorization) noexcept
{
	authorization = {};
	auto *session = findSession(sessionToken);
	const auto nowMs = clock_.nowMs();
	const auto permissionMask = static_cast<std::uint32_t>(permission);
	if (session == nullptr || !validateSession(*session, sessionToken, nowMs) ||
		!constantTimeEqual(host, expectedHost_.data()) || (session->permissions & permissionMask) != permissionMask)
	{
		return false;
	}
	if (mutation && (!constantTimeEqual(origin, expectedOrigin_.data()) ||
		!constantTimeEqual(csrfToken, session->csrfToken.data()) || !mutationAllowed(*session, permission, nowMs)))
	{
		return false;
	}
	if (!origin.empty() && !constantTimeEqual(origin, expectedOrigin_.data())) return false;
	authorization = {session->id, session->permissions};
	return true;
}

WebSecurityService::Session *WebSecurityService::findSession(const std::string_view jwt) noexcept
{
	const auto match = std::find_if(sessions_.begin(), sessions_.end(), [jwt](const auto &session) {
		return session.active && constantTimeEqual(jwt, session.jwt.data());
	});
	return match == sessions_.end() ? nullptr : &*match;
}

bool WebSecurityService::validateSession(Session &session, const std::string_view jwt, const std::uint32_t nowMs) noexcept
{
	if (!session.active || nowMs - session.issuedAtMs >= accessLifetimeMs ||
		nowMs - session.issuedAtMs >= absoluteLifetimeMs || !verifyJwt(jwt))
	{
		session = {};
		return false;
	}
	return true;
}

bool WebSecurityService::createJwt(const ports::WebUserRecord &user, Session &session) noexcept
{
	std::array<std::uint8_t, 16> jti{};
	if (!crypto_.random(jti.data(), jti.size())) return false;
	std::array<char, 33> jtiText{};
	encodeHex(jti.data(), jti.size(), jtiText.data());
	char payload[384]{};
	const auto role = user.role == ports::WebUserRole::Administrator ? "administrator" : "guest";
	const auto issuedSeconds = session.issuedAtMs / 1000U;
	const auto expiresSeconds = issuedSeconds + accessLifetimeMs / 1000U;
	const auto payloadLength = std::snprintf(payload, sizeof(payload),
		"{\"iss\":\"switch-actuator\",\"sub\":%lu,\"role\":\"%s\",\"permissions\":%lu,\"iat\":%lu,\"exp\":%lu,\"jti\":\"%s\",\"generation\":%lu,\"sid\":%lu}",
		static_cast<unsigned long>(user.id), role, static_cast<unsigned long>(session.permissions),
		static_cast<unsigned long>(issuedSeconds), static_cast<unsigned long>(expiresSeconds), jtiText.data(),
		static_cast<unsigned long>(record_.signingGeneration), static_cast<unsigned long>(session.id));
	if (payloadLength <= 0 || static_cast<std::size_t>(payloadLength) >= sizeof(payload)) return false;
	char encodedPayload[512]{};
	const auto encodedLength = base64UrlEncode(reinterpret_cast<const std::uint8_t *>(payload),
		static_cast<std::size_t>(payloadLength), encodedPayload, sizeof(encodedPayload));
	if (encodedLength == 0) return false;
	char signingInput[640]{};
	const auto signingLength = std::snprintf(signingInput, sizeof(signingInput), "%.*s.%s",
		static_cast<int>(jwtHeaderEncoded.size()), jwtHeaderEncoded.data(), encodedPayload);
	if (signingLength <= 0 || static_cast<std::size_t>(signingLength) >= sizeof(signingInput)) return false;
	std::array<std::uint8_t, 32> signature{};
	if (!crypto_.hmacSha256(record_.signingKey.data(), record_.signingKey.size(),
		std::string_view{signingInput, static_cast<std::size_t>(signingLength)}, signature.data(), signature.size())) return false;
	char encodedSignature[48]{};
	if (base64UrlEncode(signature.data(), signature.size(), encodedSignature, sizeof(encodedSignature)) == 0) return false;
	return std::snprintf(session.jwt.data(), session.jwt.size(), "%s.%s", signingInput, encodedSignature) > 0;
}

bool WebSecurityService::verifyJwt(const std::string_view jwt) const noexcept
{
	if (jwt.size() >= 768U || jwt.substr(0, jwtHeaderEncoded.size()) != jwtHeaderEncoded ||
		jwt.size() <= jwtHeaderEncoded.size() + 2U || jwt[jwtHeaderEncoded.size()] != '.') return false;
	const auto signatureSeparator = jwt.rfind('.');
	if (signatureSeparator == std::string_view::npos || signatureSeparator <= jwtHeaderEncoded.size() + 1U) return false;
	std::array<std::uint8_t, 32> signature{};
	if (!crypto_.hmacSha256(record_.signingKey.data(), record_.signingKey.size(), jwt.substr(0, signatureSeparator),
		signature.data(), signature.size())) return false;
	char encodedSignature[48]{};
	if (base64UrlEncode(signature.data(), signature.size(), encodedSignature, sizeof(encodedSignature)) == 0) return false;
	return constantTimeEqual(jwt.substr(signatureSeparator + 1U), encodedSignature);
}

bool WebSecurityService::mutationAllowed(Session &session,
	const ports::WebPermission permission,
	const std::uint32_t nowMs) noexcept
{
	if (nowMs - session.mutationWindowStartedAtMs >= mutationWindowMs)
	{
		session.mutationWindowStartedAtMs = nowMs;
		session.relayMutations = 0;
		session.configurationMutations = 0;
	}
	if (permission == ports::WebPermission::RelayCommand)
	{
		if (session.relayMutations >= 10U) return false;
		++session.relayMutations;
		return true;
	}
	if (session.configurationMutations >= 2U) return false;
	++session.configurationMutations;
	return true;
}

std::uint32_t WebSecurityService::permissionsFor(const ports::WebUserRole role) noexcept
{
	const auto readPermissions = static_cast<std::uint32_t>(ports::WebPermission::RelayRead) |
		static_cast<std::uint32_t>(ports::WebPermission::DiagnosticsRead) |
		static_cast<std::uint32_t>(ports::WebPermission::ConfigurationRead);
	return role == ports::WebUserRole::Administrator ? readPermissions |
		static_cast<std::uint32_t>(ports::WebPermission::RelayCommand) |
		static_cast<std::uint32_t>(ports::WebPermission::ConfigurationWrite) |
		static_cast<std::uint32_t>(ports::WebPermission::UsersManage) : readPermissions;
}

bool WebSecurityService::constantTimeEqual(const std::string_view left, const std::string_view right) noexcept
{
	std::size_t difference = left.size() ^ right.size();
	const auto maximum = std::max(left.size(), right.size());
	for (std::size_t index = 0; index < maximum; ++index)
	{
		const auto leftByte = index < left.size() ? static_cast<std::uint8_t>(left[index]) : 0U;
		const auto rightByte = index < right.size() ? static_cast<std::uint8_t>(right[index]) : 0U;
		difference |= leftByte ^ rightByte;
	}
	return difference == 0;
}

bool WebSecurityService::copyText(const std::string_view input, char *const output, const std::size_t capacity) noexcept
{
	if (output == nullptr || input.empty() || input.size() >= capacity) return false;
	std::fill_n(output, capacity, '\0');
	std::copy(input.begin(), input.end(), output);
	return true;
}

std::size_t WebSecurityService::base64UrlEncode(const std::uint8_t *const input,
	const std::size_t inputSize,
	char *const output,
	const std::size_t outputCapacity) noexcept
{
	constexpr char alphabet[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"};
	const auto required = (inputSize * 4U + 2U) / 3U;
	if (input == nullptr || output == nullptr || required + 1U > outputCapacity) return 0;
	std::size_t inputIndex{0};
	std::size_t outputIndex{0};
	while (inputIndex + 3U <= inputSize)
	{
		const auto value = (static_cast<std::uint32_t>(input[inputIndex]) << 16U) |
			(static_cast<std::uint32_t>(input[inputIndex + 1U]) << 8U) | input[inputIndex + 2U];
		output[outputIndex++] = alphabet[(value >> 18U) & 0x3FU];
		output[outputIndex++] = alphabet[(value >> 12U) & 0x3FU];
		output[outputIndex++] = alphabet[(value >> 6U) & 0x3FU];
		output[outputIndex++] = alphabet[value & 0x3FU];
		inputIndex += 3U;
	}
	const auto remaining = inputSize - inputIndex;
	if (remaining > 0)
	{
		const auto value = (static_cast<std::uint32_t>(input[inputIndex]) << 16U) |
			(remaining == 2U ? static_cast<std::uint32_t>(input[inputIndex + 1U]) << 8U : 0U);
		output[outputIndex++] = alphabet[(value >> 18U) & 0x3FU];
		output[outputIndex++] = alphabet[(value >> 12U) & 0x3FU];
		if (remaining == 2U) output[outputIndex++] = alphabet[(value >> 6U) & 0x3FU];
	}
	output[outputIndex] = '\0';
	return outputIndex;
}

bool WebSecurityService::recordIsValid(const ports::WebSecurityRecord &record) noexcept
{
	const auto signingKeyPresent = std::any_of(record.signingKey.begin(), record.signingKey.end(),
		[](const auto value) { return value != 0; });
	const auto administratorPresent = std::any_of(record.users.begin(), record.users.end(), [](const auto &user) {
		return user.enabled && user.role == ports::WebUserRole::Administrator && user.id != 0 &&
			user.username[0] != '\0' && user.passwordIterations >= 100'000U;
	});
	return record.schemaVersion == 1U && record.signingGeneration != 0U && signingKeyPresent &&
		administratorPresent && record.certificate[0] != '\0' && record.privateKey[0] != '\0' &&
		record.certificate.back() == '\0' && record.privateKey.back() == '\0';
}

void WebSecurityService::fillView(const Session &session,
	const std::uint32_t nowMs,
	WebSessionView &view) const noexcept
{
	view = {};
	view.userId = session.userId;
	view.username = session.username;
	view.role = session.role;
	view.permissions = session.permissions;
	view.expiresInMs = nowMs - session.issuedAtMs >= accessLifetimeMs ? 0U : accessLifetimeMs - (nowMs - session.issuedAtMs);
	view.csrfToken = session.csrfToken;
}

void WebSecurityService::revokeUserSessions(const std::uint32_t userId) noexcept
{
	for (auto &session : sessions_)
	{
		if (session.active && session.userId == userId) session = {};
	}
}
}