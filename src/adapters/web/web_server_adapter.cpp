#include "web_server_adapter.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <PsychicFileResponse.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace switch_actuator::adapters::web
{
namespace
{
constexpr char jsonContentType[]{"application/json; charset=utf-8"};
constexpr char securityPolicy[]{"default-src 'self'; connect-src 'self' wss:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'"};
constexpr char indexPath[]{"/www/index.html"};
constexpr char assetManifestPath[]{"/www/asset-manifest.json"};

void addSecurityHeaders(PsychicResponse *const response) noexcept
{
	response->addHeader("Content-Security-Policy", securityPolicy);
	response->addHeader("X-Content-Type-Options", "nosniff");
	response->addHeader("Referrer-Policy", "no-referrer");
	response->addHeader("X-Frame-Options", "DENY");
	response->addHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
}

std::string_view boundedText(const char *const text, const std::size_t capacity) noexcept
{
	return text == nullptr ? std::string_view{} : std::string_view{text, strnlen(text, capacity)};
}

const char *stateText(const domain::RelayState state) noexcept
{
	return state == domain::RelayState::On ? "on" : "off";
}

const char *trackedStatusText(const app::WebTrackedCommandStatus status) noexcept
{
	switch (status)
	{
	case app::WebTrackedCommandStatus::Queued: return "queued";
	case app::WebTrackedCommandStatus::Applied: return "applied";
	case app::WebTrackedCommandStatus::Idempotent: return "idempotent";
	case app::WebTrackedCommandStatus::Rejected: return "rejected";
	default: return "rejected";
	}
}

const char *sourceText(const domain::CommandSource source) noexcept
{
	switch (source)
	{
	case domain::CommandSource::Safety: return "safety";
	case domain::CommandSource::Button: return "button";
	case domain::CommandSource::Knx: return "knx";
	case domain::CommandSource::Modbus: return "modbus";
	case domain::CommandSource::Web: return "web";
	case domain::CommandSource::Cli: return "cli";
	case domain::CommandSource::Restore: return "restore";
	default: return "safety";
	}
}

const char *networkStateText(const ports::NetworkLifecycleState state) noexcept
{
	switch (state)
	{
	case ports::NetworkLifecycleState::Disabled: return "disabled";
	case ports::NetworkLifecycleState::ConnectingWifi: return "connecting";
	case ports::NetworkLifecycleState::OnlineWifi: return "online";
	case ports::NetworkLifecycleState::RecoveryAp: return "recovery-ap";
	default: return "disabled";
	}
}

void formatIpv4(const std::array<std::uint8_t, 4> &address, char (&output)[16]) noexcept
{
	std::snprintf(output, sizeof(output), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
}

bool hasIpv4(const std::array<std::uint8_t, 4> &address) noexcept
{
	return std::any_of(address.begin(), address.end(), [](const auto octet) { return octet != 0; });
}

bool canonicalizeManagementIdentity(const WebServerDependencies &dependencies,
	PsychicRequest *const request,
	const bool mutation,
	std::array<char, 192> &origin,
	std::array<char, 96> &host) noexcept
{
	if (dependencies.configuration == nullptr) return false;
	const auto *requestHost = request->headerCStr("Host");
	if (requestHost == nullptr || std::strlen(requestHost) >= host.size()) return false;
	char configuredHost[96]{};
	if (std::snprintf(configuredHost, sizeof(configuredHost), "%s.local",
		dependencies.configuration->active().network.hostName.data()) <= 0) return false;
	char ipv4Host[16]{};
	const auto &ipv4 = dependencies.networkStatus.snapshot().ipv4Address;
	formatIpv4(ipv4, ipv4Host);
	const auto allowedHost = std::strcmp(requestHost, configuredHost) == 0 ||
		(hasIpv4(ipv4) && std::strcmp(requestHost, ipv4Host) == 0);
	if (!allowedHost) return false;
	if (request->hasHeader("Origin"))
	{
		const auto *requestOrigin = request->headerCStr("Origin");
		char sameOrigin[192]{};
		if (requestOrigin == nullptr || std::snprintf(sameOrigin, sizeof(sameOrigin), "https://%s", requestHost) <= 0 ||
			std::strcmp(requestOrigin, sameOrigin) != 0) return false;
	}
	else if (mutation)
	{
		return false;
	}
	std::snprintf(host.data(), host.size(), "%s", configuredHost);
	std::snprintf(origin.data(), origin.size(), "https://%s", configuredHost);
	return true;
}

esp_err_t sendJson(PsychicResponse *const response, const int status, const char *const body) noexcept
{
	addSecurityHeaders(response);
	response->addHeader("Cache-Control", "no-store");
	return response->send(status, jsonContentType, body);
}

bool parseRelayAction(const char *const value, domain::RelayAction &action) noexcept
{
	if (value == nullptr) return false;
	if (std::strcmp(value, "setOn") == 0) action = domain::RelayAction::SetOn;
	else if (std::strcmp(value, "setOff") == 0) action = domain::RelayAction::SetOff;
	else if (std::strcmp(value, "toggle") == 0) action = domain::RelayAction::Toggle;
	else return false;
	return true;
}

template <std::size_t Capacity>
bool copyJsonText(const JsonVariantConst value, std::array<char, Capacity> &output) noexcept
{
	const auto *text = value.as<const char *>();
	if (text == nullptr || std::strlen(text) >= output.size()) return false;
	std::snprintf(output.data(), output.size(), "%s", text);
	return true;
}

bool parseIpv4(const JsonVariantConst value, std::array<std::uint8_t, 4> &output) noexcept
{
	const auto *text = value.as<const char *>();
	if (text == nullptr) return false;
	unsigned octets[4]{};
	char trailing{};
	if (std::sscanf(text, "%u.%u.%u.%u%c", &octets[0], &octets[1], &octets[2], &octets[3], &trailing) != 4 ||
		std::any_of(std::begin(octets), std::end(octets), [](const auto octet) { return octet > 255; })) return false;
	std::transform(std::begin(octets), std::end(octets), output.begin(),
		[](const auto octet) { return static_cast<std::uint8_t>(octet); });
	return true;
}

void formatKnxIndividualAddress(const std::uint16_t address, char (&output)[12]) noexcept
{
	std::snprintf(output, sizeof(output), "%u.%u.%u", static_cast<unsigned int>(address >> 12U),
		static_cast<unsigned int>((address >> 8U) & 0x0FU), static_cast<unsigned int>(address & 0xFFU));
}

void formatKnxGroupAddress(const std::uint16_t address, char (&output)[12]) noexcept
{
	if (address == 0)
	{
		output[0] = '\0';
		return;
	}
	std::snprintf(output, sizeof(output), "%u/%u/%u", static_cast<unsigned int>(address >> 11U),
		static_cast<unsigned int>((address >> 8U) & 0x07U), static_cast<unsigned int>(address & 0xFFU));
}

bool parseKnxIndividualAddress(const JsonVariantConst value, std::uint16_t &output) noexcept
{
	const auto *text = value.as<const char *>();
	if (text == nullptr) return false;
	if (*text == '\0')
	{
		output = 0;
		return true;
	}
	unsigned area{};
	unsigned line{};
	unsigned device{};
	char trailing{};
	if (std::sscanf(text, "%u.%u.%u%c", &area, &line, &device, &trailing) != 3 ||
		area > 15 || line > 15 || device > 255) return false;
	output = static_cast<std::uint16_t>((area << 12U) | (line << 8U) | device);
	return true;
}

bool parseKnxGroupAddress(const JsonVariantConst value, std::uint16_t &output) noexcept
{
	const auto *text = value.as<const char *>();
	if (text == nullptr) return false;
	if (*text == '\0')
	{
		output = 0;
		return true;
	}
	unsigned mainGroup{};
	unsigned middleGroup{};
	unsigned subGroup{};
	char trailing{};
	if (std::sscanf(text, "%u/%u/%u%c", &mainGroup, &middleGroup, &subGroup, &trailing) != 3 ||
		mainGroup > 31 || middleGroup > 7 || subGroup > 255) return false;
	output = static_cast<std::uint16_t>((mainGroup << 11U) | (middleGroup << 8U) | subGroup);
	return true;
}

bool parseProfileIndex(const std::string_view path, const std::string_view suffix, std::uint8_t &index) noexcept
{
	constexpr std::string_view prefix{"/api/v1/network/wifi/profiles/"};
	if (path.size() != prefix.size() + 1U + suffix.size() || path.substr(0, prefix.size()) != prefix ||
		path.substr(prefix.size() + 1U) != suffix || path[prefix.size()] < '0' || path[prefix.size()] > '9') return false;
	index = static_cast<std::uint8_t>(path[prefix.size()] - '0');
	return index < domain::wifiProfileCount;
}

const char *faultCodeText(const domain::FaultCode code) noexcept
{
	switch (code)
	{
	case domain::FaultCode::InvalidConfiguration: return "configuration.invalid";
	case domain::FaultCode::IncompatibleBoard: return "board.incompatible";
	case domain::FaultCode::RelayOutputFailure: return "relay.output_failure";
	case domain::FaultCode::CommandQueueOverflow: return "relay.queue_overflow";
	case domain::FaultCode::ModbusTransportError: return "modbus.transport_error";
	case domain::FaultCode::ModbusProtocolError: return "modbus.protocol_error";
	case domain::FaultCode::KnxUnavailable: return "knx.unavailable";
	case domain::FaultCode::KnxBusOff: return "knx.bus_off";
	case domain::FaultCode::SettingsLoadFailure: return "settings.load_failure";
	case domain::FaultCode::SettingsSaveFailure: return "settings.save_failure";
	case domain::FaultCode::TaskWatchdogFailure: return "watchdog.task_failure";
	case domain::FaultCode::WatchdogReset: return "reset.watchdog";
	case domain::FaultCode::BrownoutReset: return "reset.brownout";
	case domain::FaultCode::PanicReset: return "reset.panic";
	case domain::FaultCode::RepeatedBoot: return "reset.repeated_boot";
	case domain::FaultCode::ResourceExhaustion: return "runtime.resource_exhaustion";
	case domain::FaultCode::FileSystemFailure: return "filesystem.failure";
	default: return "fault.unknown";
	}
}

const char *faultSeverityText(const domain::FaultSeverity severity) noexcept
{
	return severity == domain::FaultSeverity::Critical ? "critical" :
		severity == domain::FaultSeverity::Warning ? "warning" : "info";
}
}

WebServerAdapter::WebServerAdapter(const WebServerDependencies dependencies) noexcept : dependencies_{dependencies}
{
}

WebServerInitializeResult WebServerAdapter::initialize() noexcept
{
	stop();
	if (dependencies_.configuration == nullptr || !dependencies_.configuration->active().web.enabled)
	{
		return WebServerInitializeResult::Disabled;
	}
	if (!dependencies_.configuration->active().web.securityProvisioned || !dependencies_.security.isValid())
	{
		return WebServerInitializeResult::SecurityUnavailable;
	}
	const auto certificate = dependencies_.security.certificate();
	const auto privateKey = dependencies_.security.privateKey();
	if (certificate.empty() || privateKey.empty() || certificate.back() != '\0' || privateKey.back() != '\0')
	{
		return WebServerInitializeResult::CertificateUnavailable;
	}
	std::snprintf(bootId_.data(), bootId_.size(), "%08lx%08lx%08lx%08lx",
		static_cast<unsigned long>(esp_random()),
		static_cast<unsigned long>(esp_random()),
		static_cast<unsigned long>(esp_random()),
		static_cast<unsigned long>(esp_random()));
	static_cast<void>(loadStaticAssetManifest());
	server_.maxRequestBodySize = maximumFrameBytes;
	server_.maxUploadSize = 0;
	server_.config.max_open_sockets = maximumWebSocketClients + 3;
	server_.config.stack_size = 8192;
	server_.setCertificate(reinterpret_cast<const std::uint8_t *>(certificate.data()), certificate.size(),
		reinterpret_cast<const std::uint8_t *>(privateKey.data()), privateKey.size());
	registerRoutes();
	if (server_.start() != ESP_OK)
	{
		return WebServerInitializeResult::StartFailure;
	}
	running_ = true;
	return WebServerInitializeResult::Initialized;
}

void WebServerAdapter::update(const std::uint32_t nowMs) noexcept
{
	if (!running_) return;
	for (auto &pending : pendingAuthorizations_)
	{
		if (pending.socket >= 0 && nowMs - pending.authorizedAtMs >= pendingAuthorizationTimeoutMs) pending = {};
	}
	publishEvents(nowMs);
}

void WebServerAdapter::stop() noexcept
{
	if (server_.isRunning()) static_cast<void>(server_.stop());
	clients_.fill({});
	pendingAuthorizations_.fill({});
	webSocketSendFailures_ = 0;
	webSocketSequenceGaps_ = 0;
	running_ = false;
}

bool WebServerAdapter::isRunning() const noexcept { return running_; }

void WebServerAdapter::registerRoutes() noexcept
{
	if (routesRegistered_) return;
	routesRegistered_ = true;
	server_.setURIMatchFunction(MATCH_WILDCARD);
	server_.on("/api/v1/session", HTTP_POST, [this](auto *request, auto *response) { return createSession(request, response); });
	server_.on("/api/v1/session", HTTP_GET, [this](auto *request, auto *response) { return sendSession(request, response); });
	server_.on("/api/v1/session", HTTP_DELETE, [this](auto *request, auto *response) { return deleteSession(request, response); });
	server_.on("/api/v1/capabilities", HTTP_GET, [this](auto *request, auto *response) { return sendCapabilities(request, response); });
	server_.on("/api/v1/device", HTTP_GET, [this](auto *request, auto *response) { return sendDevice(request, response); });
	server_.on("/api/v1/diagnostics", HTTP_GET, [this](auto *request, auto *response) { return sendDiagnostics(request, response); });
	server_.on("/api/v1/network", HTTP_GET, [this](auto *request, auto *response) { return sendNetwork(request, response); });
	server_.on("/api/v1/relays", HTTP_GET, [this](auto *request, auto *response) { return sendRelays(request, response); });
	server_.on("/api/v1/network/wifi", HTTP_GET, [this](auto *request, auto *response) { return sendWifi(request, response); });
	server_.on("/api/v1/protocols/modbus", HTTP_GET, [this](auto *request, auto *response) { return sendModbusConfiguration(request, response); });
	server_.on("/api/v1/protocols/modbus", HTTP_PUT, [this](auto *request, auto *response) { return saveModbusConfiguration(request, response); });
	server_.on("/api/v1/protocols/modbus/role", HTTP_PUT, [this](auto *request, auto *response) { return setModbusRole(request, response); });
	server_.on("/api/v1/protocols/knx", HTTP_GET, [this](auto *request, auto *response) { return sendKnxConfiguration(request, response); });
	server_.on("/api/v1/protocols/knx", HTTP_PUT, [this](auto *request, auto *response) { return saveKnxConfiguration(request, response); });
	server_.on("/api/v1/users", HTTP_GET, [this](auto *request, auto *response) { return sendUsers(request, response); });
	server_.on("/api/v1/users", HTTP_POST, [this](auto *request, auto *response) { return saveUser(request, response); });
	server_.on("/api/v1/users/*", HTTP_PUT, [this](auto *request, auto *response) { return saveUser(request, response); });
	server_.on("/api/v1/maintenance/restart", HTTP_POST, [this](auto *request, auto *response) { return requestRestart(request, response); });
	server_.on("/api/v1/network/wifi/scan", HTTP_POST, [this](auto *request, auto *response) { return startWifiScan(request, response); });
	server_.on("/api/v1/network/wifi/profiles/*/move", HTTP_POST, [this](auto *request, auto *response) { return moveWifiProfile(request, response); });
	server_.on("/api/v1/network/wifi/profiles/*/connect", HTTP_POST, [this](auto *request, auto *response) { return connectWifiProfile(request, response); });
	server_.on("/api/v1/network/wifi/profiles/*", HTTP_PUT, [this](auto *request, auto *response) { return saveWifiProfile(request, response); });
	server_.on("/api/v1/network/wifi/profiles/*", HTTP_DELETE, [this](auto *request, auto *response) { return removeWifiProfile(request, response); });
	server_.on("/api/v1/network/wifi/recovery-ap", HTTP_PUT, [this](auto *request, auto *response) { return saveRecoveryAp(request, response); });
	server_.on("/api/v1/relays/*/commands", HTTP_POST, [this](auto *request, auto *response) { return submitRelayCommand(request, response); });
	server_.on("/api/v1/commands/*", HTTP_GET, [this](auto *request, auto *response) { return getCommandResult(request, response); });
	server_.on("/api/v1/operations/*", HTTP_GET, [this](auto *request, auto *response) { return getOperationResult(request, response); });
	server_.on("/assets/*", HTTP_GET, [this](auto *request, auto *response) { return sendManifestAsset(request, response); });
	for (const auto *route : {"/", "/login", "/protocols", "/diagnostics", "/settings", "/maintenance"})
	{
		server_.on(route, HTTP_GET, [this](auto *request, auto *response) { return sendIndex(request, response); });
	}
	webSocket_.addFilter([this](PsychicRequest *request) { return authorizeWebSocket(request); });
	webSocket_.onOpen([this](auto *client) { onWebSocketOpen(client); });
	webSocket_.onClose([this](auto *client) { onWebSocketClose(client); });
	webSocket_.onFrame([this](auto *request, auto *frame) { return onWebSocketFrame(request, frame); });
	server_.on("/api/v1/ws", HTTP_GET, &webSocket_);
	server_.onNotFound([](auto *, auto *response) {
		return sendJson(response, 404, "{\"error\":{\"code\":\"route.not_found\",\"message\":\"Route not found.\"}}");
	});
}

bool WebServerAdapter::loadStaticAssetManifest() noexcept
{
	staticAssets_.fill({});
	staticAssetCount_ = 0;
	auto manifest = LittleFS.open(assetManifestPath, "r");
	if (!manifest) return false;
	JsonDocument document;
	if (deserializeJson(document, manifest) != DeserializationError::Ok || document["version"].as<int>() != 1 ||
		!document["assets"].is<JsonArrayConst>()) return false;
	const auto assets = document["assets"].as<JsonArrayConst>();
	if (assets.size() > maximumStaticAssets) return false;
	for (const auto asset : assets)
	{
		StaticAsset parsed{};
		if (!copyJsonText(asset["url"], parsed.url) || !copyJsonText(asset["file"], parsed.file) ||
			!copyJsonText(asset["gzipFile"], parsed.gzipFile) || !copyJsonText(asset["contentType"], parsed.contentType))
			return false;
		const auto url = boundedText(parsed.url.data(), parsed.url.size());
		const auto file = boundedText(parsed.file.data(), parsed.file.size());
		const auto gzipFile = boundedText(parsed.gzipFile.data(), parsed.gzipFile.size());
		if (url.substr(0, 8) != "/assets/" || file.substr(0, 12) != "/www/assets/" ||
			gzipFile.substr(0, 12) != "/www/assets/" || url.find("..") != std::string_view::npos ||
			file.find("..") != std::string_view::npos || gzipFile.find("..") != std::string_view::npos ||
			!LittleFS.exists(parsed.file.data())) return false;
		staticAssets_[staticAssetCount_++] = parsed;
	}
	return staticAssetCount_ > 0;
}

bool WebServerAdapter::authorize(PsychicRequest *const request,
	const ports::WebPermission permission,
	const bool mutation,
	ports::WebAuthorization &authorization) const noexcept
{
	if (dependencies_.networkStatus.snapshot().recoveryApActive &&
		permission != ports::WebPermission::ConfigurationRead &&
		permission != ports::WebPermission::ConfigurationWrite)
	{
		return false;
	}
	std::array<char, 1024> token{};
	auto tokenSize = token.size();
	if (request->getCookie("__Host-switch_session", token.data(), &tokenSize) != ESP_OK) return false;
	std::array<char, 192> origin{};
	std::array<char, 96> host{};
	std::array<char, 96> csrf{};
	const auto copyHeader = [request](const char *name, auto &destination) {
		const auto *value = request->headerCStr(name);
		if (value == nullptr || std::strlen(value) >= destination.size()) return false;
		std::snprintf(destination.data(), destination.size(), "%s", value);
		return true;
	};
	if (!canonicalizeManagementIdentity(dependencies_, request, mutation, origin, host)) return false;
	if (mutation && !copyHeader("X-CSRF-Token", csrf)) return false;
	return dependencies_.security.authorize(boundedText(token.data(), token.size()), boundedText(origin.data(), origin.size()),
		boundedText(host.data(), host.size()), boundedText(csrf.data(), csrf.size()), permission, mutation, authorization);
}

bool WebServerAdapter::authorizeWebSocket(PsychicRequest *const request) noexcept
{
	if (!request->hasHeader("Origin") || dependencies_.networkStatus.snapshot().recoveryApActive) return false;
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::RelayRead, false, authorization)) return false;
	if (std::any_of(clients_.begin(), clients_.end(), [&authorization](const auto &client) {
		return client.client != nullptr && client.sessionId == authorization.sessionId;
	}) || std::any_of(pendingAuthorizations_.begin(), pendingAuthorizations_.end(), [&authorization](const auto &pending) {
		return pending.socket >= 0 && pending.sessionId == authorization.sessionId;
	})) return false;
	const auto pending = std::find_if(pendingAuthorizations_.begin(), pendingAuthorizations_.end(),
		[](const auto &entry) { return entry.socket < 0; });
	if (pending == pendingAuthorizations_.end()) return false;
	*pending = {request->client()->socket(), authorization.sessionId, authorization.permissions, millis()};
	return true;
}

esp_err_t WebServerAdapter::createSession(PsychicRequest *request, PsychicResponse *response) noexcept
{
	if (dependencies_.securityService == nullptr || request->contentLength() > 1024U || request->loadBody() != ESP_OK)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid credentials.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_json\",\"message\":\"Invalid credentials.\"}}");
	const auto *username = document["username"].as<const char *>();
	const auto *password = document["password"].as<const char *>();
	if (username == nullptr || password == nullptr)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid credentials.\"}}");
	app::WebSessionCreated created{};
	const auto result = dependencies_.securityService->createSession(username, password, created);
	if (result == app::WebSessionResult::RateLimited)
	{
		response->addHeader("Retry-After", "300");
		return sendJson(response, 429, "{\"error\":{\"code\":\"session.rate_limited\",\"message\":\"Sign-in is temporarily unavailable.\"}}");
	}
	if (result == app::WebSessionResult::CapacityFull) return sendUnavailable(response, "session.capacity");
	if (result != app::WebSessionResult::Applied)
		return sendJson(response, 401, "{\"error\":{\"code\":\"session.invalid_credentials\",\"message\":\"Invalid credentials.\"}}");
	char cookie[896]{};
	std::snprintf(cookie, sizeof(cookie), "__Host-switch_session=%s; Path=/; Max-Age=900; HttpOnly; Secure; SameSite=Strict",
		created.jwt.data());
	response->addHeader("Set-Cookie", cookie);
	return sendSessionView(response, created.view);
}

esp_err_t WebServerAdapter::sendSession(PsychicRequest *request, PsychicResponse *response) noexcept
{
	if (dependencies_.securityService == nullptr) return sendUnauthorized(response);
	std::array<char, 768> token{};
	auto tokenSize = token.size();
	if (request->getCookie("__Host-switch_session", token.data(), &tokenSize) != ESP_OK) return sendUnauthorized(response);
	app::WebSessionView view{};
	if (dependencies_.securityService->inspectSession(boundedText(token.data(), token.size()), view) !=
		app::WebSessionResult::Applied) return sendUnauthorized(response);
	return sendSessionView(response, view);
}

esp_err_t WebServerAdapter::deleteSession(PsychicRequest *request, PsychicResponse *response) noexcept
{
	if (dependencies_.securityService == nullptr) return sendUnauthorized(response);
	std::array<char, 768> token{};
	std::array<char, 192> origin{};
	std::array<char, 96> host{};
	std::array<char, 96> csrf{};
	auto tokenSize = token.size();
	const auto copyHeader = [request](const char *name, auto &destination) {
		const auto *value = request->headerCStr(name);
		if (value == nullptr || std::strlen(value) >= destination.size()) return false;
		std::snprintf(destination.data(), destination.size(), "%s", value);
		return true;
	};
	if (request->getCookie("__Host-switch_session", token.data(), &tokenSize) != ESP_OK ||
		!canonicalizeManagementIdentity(dependencies_, request, true, origin, host) ||
		!copyHeader("X-CSRF-Token", csrf) ||
		dependencies_.securityService->deleteSession(boundedText(token.data(), token.size()),
			boundedText(origin.data(), origin.size()), boundedText(host.data(), host.size()),
			boundedText(csrf.data(), csrf.size())) != app::WebSessionResult::Applied)
	{
		return sendUnauthorized(response);
	}
	response->addHeader("Set-Cookie", "__Host-switch_session=; Path=/; Max-Age=0; HttpOnly; Secure; SameSite=Strict");
	addSecurityHeaders(response);
	response->addHeader("Cache-Control", "no-store");
	return response->send(204);
}

esp_err_t WebServerAdapter::sendSessionView(PsychicResponse *response, const app::WebSessionView &view) const noexcept
{
	JsonDocument document;
	document["user"]["id"] = view.userId;
	document["user"]["username"] = view.username.data();
	document["user"]["role"] = view.role == ports::WebUserRole::Administrator ? "administrator" : "guest";
	document["user"]["enabled"] = true;
	document["expiresInMs"] = view.expiresInMs;
	document["csrfToken"] = view.csrfToken.data();
	auto permissions = document["permissions"].to<JsonArray>();
	const auto append = [&permissions, &view](const ports::WebPermission permission, const char *name) {
		if ((view.permissions & static_cast<std::uint32_t>(permission)) != 0U) permissions.add(name);
	};
	append(ports::WebPermission::RelayRead, "relay:read");
	append(ports::WebPermission::RelayCommand, "relay:command");
	append(ports::WebPermission::DiagnosticsRead, "diagnostics:read");
	append(ports::WebPermission::ConfigurationRead, "configuration:read");
	append(ports::WebPermission::ConfigurationWrite, "configuration:write");
	append(ports::WebPermission::UsersManage, "users:manage");
	append(ports::WebPermission::FirmwareUpdate, "firmware:update");
	char body[1024]{};
	if (serializeJson(document, body, sizeof(body)) >= sizeof(body) - 1U) return sendUnavailable(response, "response.capacity");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendCapabilities(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::RelayRead, false, authorization)) return sendUnauthorized(response);
	JsonDocument document;
	document["apiVersion"] = "1.0";
	document["minimumUiVersion"] = "1.0.0";
	document["deviceId"] = "local-device";
	document["bootId"] = bootId_.data();
	document["model"] = "Waveshare ESP32-S3 Relay 6CH";
	auto channels = document["channels"].to<JsonArray>();
	for (std::uint8_t channel = 0; channel < domain::relayChannelCount; ++channel)
	{
		auto item = channels.add<JsonObject>();
		item["id"] = channel;
		char label[8]{};
		std::snprintf(label, sizeof(label), "CH%u", channel + 1U);
		item["physicalLabel"] = label;
		item["contactFeedback"] = false;
	}
	document["features"]["wifi"] = true;
	document["features"]["ethernet"] = false;
	document["features"]["modbus"] = true;
	document["features"]["knx"] = true;
	document["features"]["scenes"] = false;
	document["features"]["timers"] = false;
	document["features"]["remoteRestart"] = true;
	document["features"]["remoteFactoryReset"] = false;
	document["features"]["firmwareUpdate"] = false;
	auto permissions = document["permissions"].to<JsonArray>();
	const auto append = [&permissions, &authorization](const ports::WebPermission permission, const char *name) {
		if ((authorization.permissions & static_cast<std::uint32_t>(permission)) != 0U) permissions.add(name);
	};
	append(ports::WebPermission::RelayRead, "relay:read");
	append(ports::WebPermission::RelayCommand, "relay:command");
	append(ports::WebPermission::DiagnosticsRead, "diagnostics:read");
	append(ports::WebPermission::ConfigurationRead, "configuration:read");
	append(ports::WebPermission::ConfigurationWrite, "configuration:write");
	append(ports::WebPermission::UsersManage, "users:manage");
	append(ports::WebPermission::FirmwareUpdate, "firmware:update");
	char body[2048]{};
	if (serializeJson(document, body, sizeof(body)) >= sizeof(body) - 1U) return sendUnavailable(response, "response.capacity");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendDevice(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::DiagnosticsRead, false, authorization) || dependencies_.diagnostics == nullptr) return sendUnauthorized(response);
	const auto &snapshot = dependencies_.diagnostics->snapshot();
	char body[768]{};
	std::snprintf(body, sizeof(body), "{\"name\":\"Switch actuator\",\"model\":\"%s\",\"serialSuffix\":\"local\",\"firmwareVersion\":\"%s\",\"buildId\":\"%s\",\"uptimeMs\":%lu,\"lifecycle\":\"%s\",\"lifecycleReason\":\"runtime\",\"configurationGeneration\":%lu}", snapshot.boardModel.data(), snapshot.firmwareVersion.data(), snapshot.buildId.data(), static_cast<unsigned long>(snapshot.uptimeMs), snapshot.lifecycleState == app::LifecycleState::Operational ? "operational" : "degraded", static_cast<unsigned long>(snapshot.configurationGeneration));
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendDiagnostics(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::DiagnosticsRead, false, authorization) || dependencies_.diagnostics == nullptr ||
		dependencies_.configuration == nullptr) return sendUnauthorized(response);
	const auto &snapshot = dependencies_.diagnostics->snapshot();
	const auto &configuration = dependencies_.configuration->active();
	JsonDocument document;
	document["configurationValid"] = snapshot.configurationValid;
	document["persistenceHealthy"] = snapshot.lastSettingsSaveResult == ports::SettingsSaveResult::Saved;
	document["taskWatchdogHealthy"] = snapshot.taskWatchdogHealthy;
	document["heapLowWaterMarkBytes"] = snapshot.heapLowWaterMarkBytes;
	document["largestFreeHeapBlockBytes"] = ESP.getMaxAllocHeap();
	document["commandCounters"]["accepted"] = snapshot.commands.accepted;
	document["commandCounters"]["rejected"] = snapshot.commands.rejected;
	document["commandCounters"]["queueFull"] = snapshot.commands.queueFull;
	const auto activeClients = std::count_if(clients_.begin(), clients_.end(), [](const auto &client) {
		return client.client != nullptr;
	});
	document["web"]["activeClients"] = activeClients;
	document["web"]["maximumClients"] = maximumWebSocketClients;
	document["web"]["sendFailures"] = webSocketSendFailures_;
	document["web"]["sequenceGaps"] = webSocketSequenceGaps_;
	document["web"]["coalescedEvents"] = 0;
	if (dependencies_.requestQueue != nullptr)
	{
		document["web"]["requestQueueDepth"] = dependencies_.requestQueue->size();
		document["web"]["requestQueueHighWaterMark"] = dependencies_.requestQueue->highWaterMark();
		document["web"]["requestQueueCapacity"] = app::WebRequestQueue::capacity;
	}
	auto faults = document["faults"].to<JsonArray>();
	for (const auto &fault : snapshot.faults)
	{
		if (!fault.active) continue;
		auto item = faults.add<JsonObject>();
		item["code"] = faultCodeText(fault.code);
		item["severity"] = faultSeverityText(fault.severity);
		item["summary"] = faultCodeText(fault.code);
		item["occurrenceCount"] = fault.occurrenceCount;
	}
	document["protocols"]["modbus"]["available"] = true;
	document["protocols"]["modbus"]["unitId"] = configuration.modbus.unitId;
	document["protocols"]["modbus"]["baudRate"] = configuration.modbus.baudRate;
	document["protocols"]["modbus"]["validRequests"] = snapshot.modbus.validRequests;
	document["protocols"]["modbus"]["errors"] = snapshot.modbus.crcErrors + snapshot.modbus.malformedFrames +
		snapshot.modbus.illegalFunction + snapshot.modbus.illegalAddress + snapshot.modbus.illegalValue + snapshot.modbus.timeouts;
	document["protocols"]["knx"]["available"] = snapshot.knx.available;
	document["protocols"]["knx"]["enabled"] = configuration.knx.enabled;
	document["protocols"]["knx"]["busOnline"] = snapshot.knx.busOnline;
	if (configuration.knx.individualAddress == 0) document["protocols"]["knx"]["individualAddress"] = nullptr;
	else
	{
		char address[16]{};
		std::snprintf(address, sizeof(address), "%u.%u.%u", (configuration.knx.individualAddress >> 12U) & 0x0FU,
			(configuration.knx.individualAddress >> 8U) & 0x0FU, configuration.knx.individualAddress & 0xFFU);
		document["protocols"]["knx"]["individualAddress"] = address;
	}
	document["protocols"]["knx"]["validTelegrams"] = snapshot.knx.validTelegrams;
	document["protocols"]["knx"]["errors"] = snapshot.knx.telegramErrors;
	char body[3072]{};
	if (serializeJson(document, body, sizeof(body)) >= sizeof(body) - 1U) return sendUnavailable(response, "response.capacity");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendNetwork(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::DiagnosticsRead, false, authorization)) return sendUnauthorized(response);
	const auto &snapshot = dependencies_.networkStatus.snapshot();
	char address[16]{};
	formatIpv4(snapshot.ipv4Address, address);
	char body[512]{};
	if (snapshot.activeProfileIndex == 0xFF)
		std::snprintf(body, sizeof(body), "{\"state\":\"%s\",\"ipv4Address\":%s%s%s,\"rssi\":%ld,\"activeProfileIndex\":null,\"recoveryApActive\":%s,\"lastConnectedAgeMs\":null}", networkStateText(snapshot.state), hasIpv4(snapshot.ipv4Address) ? "\"" : "", hasIpv4(snapshot.ipv4Address) ? address : "null", hasIpv4(snapshot.ipv4Address) ? "\"" : "", static_cast<long>(snapshot.rssi), snapshot.recoveryApActive ? "true" : "false");
	else
		std::snprintf(body, sizeof(body), "{\"state\":\"%s\",\"ipv4Address\":%s%s%s,\"rssi\":%ld,\"activeProfileIndex\":%u,\"recoveryApActive\":%s,\"lastConnectedAgeMs\":null}", networkStateText(snapshot.state), hasIpv4(snapshot.ipv4Address) ? "\"" : "", hasIpv4(snapshot.ipv4Address) ? address : "null", hasIpv4(snapshot.ipv4Address) ? "\"" : "", static_cast<long>(snapshot.rssi), snapshot.activeProfileIndex, snapshot.recoveryApActive ? "true" : "false");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendRelays(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::RelayRead, false, authorization) || dependencies_.relayService == nullptr) return sendUnauthorized(response);
	char body[3072]{};
	std::size_t used = static_cast<std::size_t>(std::snprintf(body, sizeof(body), "{\"bootId\":\"%s\",\"snapshotSequence\":%lu,\"relays\":[", bootId_.data(), dependencies_.events != nullptr ? static_cast<unsigned long>(dependencies_.events->latestSequence()) : 0UL));
	const auto &snapshots = dependencies_.relayService->snapshots();
	for (std::size_t index = 0; index < snapshots.size() && used < sizeof(body); ++index)
	{
		const auto &relay = snapshots[index];
		used += static_cast<std::size_t>(std::snprintf(body + used, sizeof(body) - used, "%s{\"id\":%u,\"physicalLabel\":\"CH%u\",\"requestedState\":\"%s\",\"appliedState\":\"%s\",\"verification\":\"gpio-write\",\"lastSource\":\"%s\",\"transitionSequence\":%lu,\"lastTransitionAgeMs\":0,\"fault\":%s,\"lockedOut\":%s,\"enabled\":true}", index == 0 ? "" : ",", static_cast<unsigned>(index), static_cast<unsigned>(index + 1), stateText(relay.requestedState), stateText(relay.appliedState), sourceText(relay.lastCommandSource), static_cast<unsigned long>(relay.transitionSequence), relay.fault == domain::RelayFault::None ? "null" : "\"Output unavailable\"", relay.lockedOut ? "true" : "false"));
	}
	if (used + 3 >= sizeof(body)) return sendUnavailable(response, "response.capacity");
	std::snprintf(body + used, sizeof(body) - used, "]}");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendWifi(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationRead, false, authorization) || dependencies_.wifiManagement == nullptr) return sendUnauthorized(response);
	const auto wifi = dependencies_.wifiManagement->snapshot();
	const auto &network = dependencies_.networkStatus.snapshot();
	JsonDocument document;
	document["generation"] = wifi.generation;
	if (network.activeProfileIndex == 0xFF) document["activeProfileIndex"] = nullptr;
	else document["activeProfileIndex"] = network.activeProfileIndex;
	const char *scanState = network.wifiScan.state == ports::WifiScanState::Scanning ? "scanning" :
		network.wifiScan.state == ports::WifiScanState::Complete ? "complete" :
		network.wifiScan.state == ports::WifiScanState::Failed ? "failed" : "idle";
	document["scan"]["state"] = scanState;
	document["scan"]["sequence"] = network.wifiScan.sequence;
	auto results = document["scan"]["results"].to<JsonArray>();
	for (std::size_t index = 0; index < network.wifiScan.resultCount; ++index)
	{
		auto result = results.add<JsonObject>();
		result["ssid"] = network.wifiScan.results[index].ssid.data();
		result["rssi"] = network.wifiScan.results[index].rssi;
		result["channel"] = network.wifiScan.results[index].channel;
		result["secured"] = network.wifiScan.results[index].secured;
	}
	auto profiles = document["profiles"].to<JsonArray>();
	for (const auto &profile : wifi.profiles)
	{
		auto item = profiles.add<JsonObject>();
		item["index"] = profile.index;
		item["enabled"] = profile.enabled;
		item["ssid"] = profile.ssid.data();
		item["hasPassphrase"] = profile.hasPassphrase;
		item["ipv4"]["mode"] = profile.ipv4.mode == domain::IpMode::Dhcp ? "dhcp" : "static";
		item["ipv4"]["address"] = "";
		item["ipv4"]["subnetMask"] = "";
		item["ipv4"]["gateway"] = "";
		item["ipv4"]["dns"] = "";
	}
	document["recoveryAp"]["enabled"] = wifi.recoveryAp.enabled;
	document["recoveryAp"]["ssidPrefix"] = wifi.recoveryAp.ssidPrefix.data();
	document["recoveryAp"]["channel"] = wifi.recoveryAp.channel;
	document["recoveryAp"]["timeoutMs"] = wifi.recoveryAp.timeoutMs;
	document["recoveryAp"]["remainActiveWhileOffline"] = wifi.recoveryAp.remainActiveWhileOffline;
	document["recoveryAp"]["active"] = network.recoveryApActive;
	char body[4096]{};
	if (serializeJson(document, body, sizeof(body)) >= sizeof(body) - 1U) return sendUnavailable(response, "response.capacity");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendModbusConfiguration(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationRead, false, authorization) ||
		dependencies_.configuration == nullptr || !dependencies_.modbusControl.isValid()) return sendUnauthorized(response);
	const auto &configuration = dependencies_.configuration->active();
	const auto &modbus = configuration.modbus;
	const auto *parity = modbus.parity == domain::SerialParity::None ? "none" :
		modbus.parity == domain::SerialParity::Even ? "even" : "odd";
	char body[256]{};
	std::snprintf(body, sizeof(body),
		"{\"generation\":%lu,\"role\":\"%s\",\"unitId\":%u,\"baudRate\":%lu,\"parity\":\"%s\",\"dataBits\":%u,\"stopBits\":%u}",
		static_cast<unsigned long>(configuration.generation),
		dependencies_.modbusControl.role(dependencies_.modbusControl.context) == ports::ModbusRtuRole::Server ?
			"server" : "client",
		static_cast<unsigned int>(modbus.unitId),
		static_cast<unsigned long>(modbus.baudRate), parity, static_cast<unsigned int>(modbus.dataBits),
		static_cast<unsigned int>(modbus.stopBits));
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::saveModbusConfiguration(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (request->contentLength() > 512U || request->loadBody() != ESP_OK)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid Modbus configuration.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok ||
		!document["unitId"].is<std::uint8_t>() || !document["baudRate"].is<std::uint32_t>() ||
		!document["stopBits"].is<std::uint8_t>() || !document["expectedGeneration"].is<std::uint32_t>())
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_json\",\"message\":\"Invalid Modbus configuration.\"}}");
	const auto *parity = document["parity"].as<const char *>();
	if (parity == nullptr || (std::strcmp(parity, "none") != 0 && std::strcmp(parity, "even") != 0 &&
		std::strcmp(parity, "odd") != 0))
		return sendJson(response, 422, "{\"error\":{\"code\":\"modbus.invalid_parity\",\"message\":\"Invalid serial parity.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::SaveModbusConfiguration;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.expectedGeneration = document["expectedGeneration"];
	operation.modbusConfiguration.unitId = document["unitId"];
	operation.modbusConfiguration.baudRate = document["baudRate"];
	operation.modbusConfiguration.parity = std::strcmp(parity, "none") == 0 ? domain::SerialParity::None :
		std::strcmp(parity, "even") == 0 ? domain::SerialParity::Even : domain::SerialParity::Odd;
	operation.modbusConfiguration.dataBits = 8;
	operation.modbusConfiguration.stopBits = document["stopBits"];
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::setModbusRole(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (request->contentLength() > 128U || request->loadBody() != ESP_OK)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid Modbus role.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_json\",\"message\":\"Invalid Modbus role.\"}}");
	const auto *role = document["role"].as<const char *>();
	if (role == nullptr || (std::strcmp(role, "server") != 0 && std::strcmp(role, "client") != 0))
		return sendJson(response, 422, "{\"error\":{\"code\":\"modbus.invalid_role\",\"message\":\"Invalid Modbus role.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::SetModbusRole;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.modbusRole = std::strcmp(role, "server") == 0 ?
		ports::ModbusRtuRole::Server : ports::ModbusRtuRole::Client;
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::sendKnxConfiguration(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationRead, false, authorization) ||
		dependencies_.configuration == nullptr) return sendUnauthorized(response);
	const auto &active = dependencies_.configuration->active();
	const auto &knx = active.knx;
	JsonDocument document;
	document["generation"] = active.generation;
	document["enabled"] = knx.enabled;
	char address[12]{};
	formatKnxIndividualAddress(knx.individualAddress, address);
	document["individualAddress"] = knx.individualAddress == 0 ? "" : address;
	document["startupTransmitDelayMs"] = knx.startupTransmitDelayMs;
	document["minimumTelegramIntervalMs"] = knx.minimumTelegramIntervalMs;
	document["cyclicStatusIntervalMs"] = knx.cyclicStatusIntervalMs;
	document["heartbeatIntervalMs"] = knx.heartbeatIntervalMs;
	document["readSwitchObject"] = knx.readSwitchObject;
	const auto addGroupAddress = [&document, &address](const char *const key, const std::uint16_t value) {
		formatKnxGroupAddress(value, address);
		document[key] = address;
	};
	addGroupAddress("heartbeatGroupAddress", knx.heartbeatGroupAddress);
	addGroupAddress("centralSwitchGroupAddress", knx.centralSwitchGroupAddress);
	addGroupAddress("centralOffGroupAddress", knx.centralOffGroupAddress);
	addGroupAddress("deviceFaultGroupAddress", knx.deviceFaultGroupAddress);
	auto channels = document["channels"].to<JsonArray>();
	for (std::size_t index = 0; index < knx.channels.size(); ++index)
	{
		const auto &channel = knx.channels[index];
		auto item = channels.add<JsonObject>();
		item["index"] = index;
		formatKnxGroupAddress(channel.switchGroupAddress, address);
		item["switchGroupAddress"] = address;
		formatKnxGroupAddress(channel.statusGroupAddress, address);
		item["statusGroupAddress"] = address;
		formatKnxGroupAddress(channel.faultGroupAddress, address);
		item["faultGroupAddress"] = address;
		item["commandPolarityInverted"] = channel.commandPolarityInverted;
		item["statusPolarityInverted"] = channel.statusPolarityInverted;
		item["sendStatusAfterStartup"] = channel.sendStatusAfterStartup;
		item["participatesInCentralSwitch"] = channel.participatesInCentralSwitch;
		item["participatesInCentralOff"] = channel.participatesInCentralOff;
	}
	char body[4096]{};
	if (serializeJson(document, body, sizeof(body)) >= sizeof(body) - 1U) return sendUnavailable(response, "response.capacity");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::saveKnxConfiguration(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (request->contentLength() > 4096U || request->loadBody() != ESP_OK)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid KNX configuration.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok ||
		!document["expectedGeneration"].is<std::uint32_t>() || !document["enabled"].is<bool>() ||
		!document["startupTransmitDelayMs"].is<std::uint32_t>() ||
		!document["minimumTelegramIntervalMs"].is<std::uint16_t>() ||
		!document["cyclicStatusIntervalMs"].is<std::uint32_t>() ||
		!document["heartbeatIntervalMs"].is<std::uint32_t>() || !document["readSwitchObject"].is<bool>() ||
		!document["channels"].is<JsonArrayConst>())
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_json\",\"message\":\"Invalid KNX configuration.\"}}");
	const auto channels = document["channels"].as<JsonArrayConst>();
	if (channels.size() != domain::relayChannelCount)
		return sendJson(response, 422, "{\"error\":{\"code\":\"knx.invalid_channels\",\"message\":\"Exactly six KNX channels are required.\"}}");
	domain::KnxConfiguration configuration{};
	configuration.enabled = document["enabled"];
	configuration.startupTransmitDelayMs = document["startupTransmitDelayMs"];
	configuration.minimumTelegramIntervalMs = document["minimumTelegramIntervalMs"];
	configuration.cyclicStatusIntervalMs = document["cyclicStatusIntervalMs"];
	configuration.heartbeatIntervalMs = document["heartbeatIntervalMs"];
	configuration.readSwitchObject = document["readSwitchObject"];
	if (!parseKnxIndividualAddress(document["individualAddress"], configuration.individualAddress) ||
		!parseKnxGroupAddress(document["heartbeatGroupAddress"], configuration.heartbeatGroupAddress) ||
		!parseKnxGroupAddress(document["centralSwitchGroupAddress"], configuration.centralSwitchGroupAddress) ||
		!parseKnxGroupAddress(document["centralOffGroupAddress"], configuration.centralOffGroupAddress) ||
		!parseKnxGroupAddress(document["deviceFaultGroupAddress"], configuration.deviceFaultGroupAddress))
		return sendJson(response, 422, "{\"error\":{\"code\":\"knx.invalid_address\",\"message\":\"Invalid KNX address.\"}}");
	std::size_t index{0};
	for (const auto value : channels)
	{
		const auto channel = value.as<JsonObjectConst>();
		if (channel.isNull() || !channel["commandPolarityInverted"].is<bool>() ||
			!channel["statusPolarityInverted"].is<bool>() || !channel["sendStatusAfterStartup"].is<bool>() ||
			!channel["participatesInCentralSwitch"].is<bool>() || !channel["participatesInCentralOff"].is<bool>())
			return sendJson(response, 400, "{\"error\":{\"code\":\"knx.invalid_channel\",\"message\":\"Invalid KNX channel.\"}}");
		auto &target = configuration.channels[index++];
		if (!parseKnxGroupAddress(channel["switchGroupAddress"], target.switchGroupAddress) ||
			!parseKnxGroupAddress(channel["statusGroupAddress"], target.statusGroupAddress) ||
			!parseKnxGroupAddress(channel["faultGroupAddress"], target.faultGroupAddress))
			return sendJson(response, 422, "{\"error\":{\"code\":\"knx.invalid_address\",\"message\":\"Invalid KNX group address.\"}}");
		target.commandPolarityInverted = channel["commandPolarityInverted"];
		target.statusPolarityInverted = channel["statusPolarityInverted"];
		target.sendStatusAfterStartup = channel["sendStatusAfterStartup"];
		target.participatesInCentralSwitch = channel["participatesInCentralSwitch"];
		target.participatesInCentralOff = channel["participatesInCentralOff"];
	}
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::SaveKnxConfiguration;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.expectedGeneration = document["expectedGeneration"];
	operation.knxConfiguration = configuration;
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::sendUsers(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::UsersManage, false, authorization) ||
		dependencies_.securityService == nullptr) return sendUnauthorized(response);
	std::array<app::WebUserView, ports::webUserCapacity> users{};
	const auto count = dependencies_.securityService->users(users);
	JsonDocument document;
	auto output = document.to<JsonArray>();
	for (std::size_t index = 0; index < count; ++index)
	{
		auto item = output.add<JsonObject>();
		item["id"] = users[index].id;
		item["username"] = users[index].username.data();
		item["role"] = users[index].role == ports::WebUserRole::Administrator ? "administrator" : "guest";
		item["enabled"] = users[index].enabled;
	}
	char body[1024]{};
	if (serializeJson(document, body, sizeof(body)) >= sizeof(body) - 1U) return sendUnavailable(response, "response.capacity");
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::saveUser(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::UsersManage, true, authorization)) return sendUnauthorized(response);
	if (request->contentLength() > 1024U || request->loadBody() != ESP_OK)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid user.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok ||
		!document["enabled"].is<bool>()) return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid user.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::SaveUser;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.userEnabled = document["enabled"];
	const auto *role = document["role"].as<const char *>();
	if (!copyJsonText(document["username"], operation.username) || role == nullptr ||
		(std::strcmp(role, "administrator") != 0 && std::strcmp(role, "guest") != 0))
		return sendJson(response, 422, "{\"error\":{\"code\":\"user.invalid\",\"message\":\"Invalid user.\"}}");
	operation.userRole = std::strcmp(role, "administrator") == 0 ? ports::WebUserRole::Administrator :
		ports::WebUserRole::Guest;
	if (!document["password"].isNull())
	{
		operation.replacePassword = true;
		if (!copyJsonText(document["password"], operation.password))
			return sendJson(response, 422, "{\"error\":{\"code\":\"user.invalid_password\",\"message\":\"Invalid password.\"}}");
	}
	const auto path = boundedText(request->pathCStr(), 96);
	if (path != "/api/v1/users")
	{
		constexpr std::string_view prefix{"/api/v1/users/"};
		unsigned long userId{};
		char trailing{};
		if (path.substr(0, prefix.size()) != prefix || std::sscanf(path.data() + prefix.size(), "%lu%c", &userId, &trailing) != 1)
			return sendJson(response, 404, "{\"error\":{\"code\":\"user.not_found\",\"message\":\"User not found.\"}}");
		operation.userId = static_cast<std::uint32_t>(userId);
	}
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::requestRestart(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::Restart;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::startWifiScan(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::WifiScan;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::saveWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (dependencies_.wifiManagement == nullptr || request->contentLength() > maximumFrameBytes || request->loadBody() != ESP_OK)
		return sendUnavailable(response, "wifi.unavailable");
	std::uint8_t index{};
	if (!parseProfileIndex(boundedText(request->pathCStr(), 128), {}, index)) return sendJson(response, 404, "{\"error\":{\"code\":\"wifi.profile_not_found\",\"message\":\"Wi-Fi profile not found.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok || !document["enabled"].is<bool>() ||
		!document["expectedGeneration"].is<std::uint32_t>()) return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_json\",\"message\":\"Invalid Wi-Fi profile.\"}}");
	app::WifiProfilePatch patch{};
	patch.index = index;
	patch.enabled = document["enabled"];
	patch.expectedGeneration = document["expectedGeneration"];
	if (!copyJsonText(document["ssid"], patch.ssid)) return sendJson(response, 422, "{\"error\":{\"code\":\"wifi.invalid_ssid\",\"message\":\"Invalid SSID.\"}}");
	if (document["clearPassphrase"] | false) patch.passphraseUpdate = app::WifiSecretUpdate::Clear;
	else if (!document["passphrase"].isNull())
	{
		patch.passphraseUpdate = app::WifiSecretUpdate::Replace;
		if (!copyJsonText(document["passphrase"], patch.passphrase)) return sendJson(response, 422, "{\"error\":{\"code\":\"wifi.invalid_passphrase\",\"message\":\"Invalid passphrase.\"}}");
	}
	const auto ipv4 = document["ipv4"];
	const auto *mode = ipv4["mode"].as<const char *>();
	if (mode == nullptr) return sendJson(response, 422, "{\"error\":{\"code\":\"wifi.invalid_ipv4\",\"message\":\"Invalid IPv4 configuration.\"}}");
	if (std::strcmp(mode, "dhcp") == 0) patch.ipv4.mode = domain::IpMode::Dhcp;
	else if (std::strcmp(mode, "static") == 0)
	{
		patch.ipv4.mode = domain::IpMode::Static;
		if (!parseIpv4(ipv4["address"], patch.ipv4.address) || !parseIpv4(ipv4["subnetMask"], patch.ipv4.subnetMask) ||
			!parseIpv4(ipv4["gateway"], patch.ipv4.gateway) || !parseIpv4(ipv4["dns"], patch.ipv4.dns))
			return sendJson(response, 422, "{\"error\":{\"code\":\"wifi.invalid_ipv4\",\"message\":\"Invalid IPv4 configuration.\"}}");
	}
	else return sendJson(response, 422, "{\"error\":{\"code\":\"wifi.invalid_ipv4\",\"message\":\"Invalid IPv4 configuration.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::WifiSaveProfile;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.wifiProfile = patch;
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::removeWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (dependencies_.wifiManagement == nullptr || request->contentLength() > maximumFrameBytes || request->loadBody() != ESP_OK)
		return sendUnavailable(response, "wifi.unavailable");
	std::uint8_t index{};
	JsonDocument document;
	if (!parseProfileIndex(boundedText(request->pathCStr(), 128), {}, index) ||
		deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok || !document["expectedGeneration"].is<std::uint32_t>())
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid profile removal.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::WifiRemoveProfile;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.index = index;
	operation.expectedGeneration = document["expectedGeneration"];
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::moveWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (dependencies_.wifiManagement == nullptr || request->contentLength() > maximumFrameBytes || request->loadBody() != ESP_OK)
		return sendUnavailable(response, "wifi.unavailable");
	std::uint8_t index{};
	JsonDocument document;
	if (!parseProfileIndex(boundedText(request->pathCStr(), 128), "/move", index) ||
		deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok || !document["toIndex"].is<std::uint8_t>() ||
		!document["expectedGeneration"].is<std::uint32_t>()) return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid profile move.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::WifiMoveProfile;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.index = index;
	operation.toIndex = document["toIndex"];
	operation.expectedGeneration = document["expectedGeneration"];
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::connectWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	std::uint8_t index{};
	if (!parseProfileIndex(boundedText(request->pathCStr(), 128), "/connect", index)) return sendJson(response, 404, "{\"error\":{\"code\":\"wifi.profile_not_found\",\"message\":\"Wi-Fi profile not found.\"}}");
	const auto nowMs = millis();
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::WifiConnectProfile;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.index = index;
	operation.receivedAtMs = nowMs;
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::saveRecoveryAp(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationWrite, true, authorization)) return sendUnauthorized(response);
	if (dependencies_.wifiManagement == nullptr || request->contentLength() > maximumFrameBytes || request->loadBody() != ESP_OK)
		return sendUnavailable(response, "wifi.unavailable");
	JsonDocument document;
	domain::RecoveryApConfiguration configuration{};
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok || !document["enabled"].is<bool>() ||
		!document["channel"].is<std::uint8_t>() || !document["timeoutMs"].is<std::uint32_t>() ||
		!document["remainActiveWhileOffline"].is<bool>() || !document["expectedGeneration"].is<std::uint32_t>() ||
		!copyJsonText(document["ssidPrefix"], configuration.ssidPrefix))
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid\",\"message\":\"Invalid recovery access point configuration.\"}}");
	configuration.enabled = document["enabled"];
	configuration.channel = document["channel"];
	configuration.timeoutMs = document["timeoutMs"];
	configuration.remainActiveWhileOffline = document["remainActiveWhileOffline"];
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::WifiSaveRecoveryAp;
	operation.operationId = esp_random();
	operation.sessionId = authorization.sessionId;
	operation.recoveryAp = configuration;
	operation.expectedGeneration = document["expectedGeneration"];
	operation.receivedAtMs = millis();
	return enqueueOperation(response, operation);
}

esp_err_t WebServerAdapter::sendWifiMutationResult(PsychicRequest *request,
	PsychicResponse *response,
	const app::WifiManagementResult result) noexcept
{
	if (result == app::WifiManagementResult::Applied)
	{
		dependencies_.networkControl.applyCommittedConfiguration(millis());
		return sendWifi(request, response);
	}
	if (result == app::WifiManagementResult::GenerationConflict) return sendJson(response, 409, "{\"error\":{\"code\":\"configuration.generation_conflict\",\"message\":\"Configuration changed; refresh and retry.\"}}");
	if (result == app::WifiManagementResult::InvalidIndex) return sendJson(response, 404, "{\"error\":{\"code\":\"wifi.profile_not_found\",\"message\":\"Wi-Fi profile not found.\"}}");
	if (result == app::WifiManagementResult::InvalidConfiguration) return sendJson(response, 422, "{\"error\":{\"code\":\"wifi.invalid_configuration\",\"message\":\"Invalid Wi-Fi configuration.\"}}");
	return sendUnavailable(response, "configuration.persistence_failure");
}

esp_err_t WebServerAdapter::submitRelayCommand(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::RelayCommand, true, authorization)) return sendUnauthorized(response);
	if (dependencies_.switchingPolicy == nullptr || dependencies_.commandTracker == nullptr ||
		request->contentLength() > maximumFrameBytes || request->loadBody() != ESP_OK) return sendUnavailable(response, "command.unavailable");
	const auto path = boundedText(request->pathCStr(), 96);
	const auto prefix = std::string_view{"/api/v1/relays/"};
	if (path.size() <= prefix.size() || path.substr(0, prefix.size()) != prefix) return sendUnavailable(response, "command.invalid_channel");
	const auto channel = static_cast<std::uint8_t>(path[prefix.size()] - '0');
	if (channel >= domain::relayChannelCount) return sendJson(response, 404, "{\"error\":{\"code\":\"relay.not_found\",\"message\":\"Relay not found.\"}}");
	JsonDocument document;
	if (deserializeJson(document, request->bodyCStr()) != DeserializationError::Ok ||
		!document["expectedSequence"].is<std::uint32_t>()) return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_json\",\"message\":\"Invalid JSON.\"}}");
	domain::RelayAction action{};
	if (!parseRelayAction(document["action"], action)) return sendJson(response, 422, "{\"error\":{\"code\":\"relay.invalid_action\",\"message\":\"Invalid relay action.\"}}");
	const auto *idempotencyKey = request->headerCStr("Idempotency-Key");
	if (idempotencyKey == nullptr) return sendJson(response, 400, "{\"error\":{\"code\":\"request.idempotency_key_required\",\"message\":\"Idempotency key required.\"}}");
	const auto correlationId = esp_random();
	app::WebTrackedCommand tracked{};
	const auto beginResult = dependencies_.commandTracker->begin(authorization.sessionId, idempotencyKey, {channel}, action,
		document["expectedSequence"], correlationId, millis(), tracked);
	if (beginResult == app::WebCommandBeginResult::Duplicate) return sendCommandResult(response, tracked);
	if (beginResult == app::WebCommandBeginResult::IdempotencyMismatch)
		return sendJson(response, 409, "{\"error\":{\"code\":\"web.idempotency_mismatch\",\"message\":\"Idempotency key was reused for a different request.\"}}");
	if (beginResult == app::WebCommandBeginResult::CapacityFull)
		return sendJson(response, 429, "{\"error\":{\"code\":\"web.command_capacity\",\"message\":\"Command result capacity is full.\"}}");
	if (beginResult != app::WebCommandBeginResult::Accepted)
		return sendJson(response, 400, "{\"error\":{\"code\":\"request.invalid_idempotency_key\",\"message\":\"Invalid idempotency key.\"}}");
	app::WebApplicationRequest operation{};
	operation.type = app::WebRequestType::RelayCommand;
	operation.operationId = correlationId;
	operation.sessionId = authorization.sessionId;
	operation.correlationId = correlationId;
	operation.channel = {channel};
	operation.action = action;
	operation.receivedAtMs = millis();
	if (dependencies_.requestQueue == nullptr || !dependencies_.requestQueue->enqueue(operation))
	{
		static_cast<void>(dependencies_.commandTracker->reject(correlationId, app::RelayCommandReason::EventRejected,
			operation.receivedAtMs, tracked));
		return sendCommandResult(response, tracked, 429);
	}
	return sendCommandResult(response, tracked, 202);
}

esp_err_t WebServerAdapter::sendCommandResult(PsychicResponse *response,
	const app::WebTrackedCommand &command,
	const int status) const noexcept
{
	char body[320]{};
	std::snprintf(body, sizeof(body),
		"{\"correlationId\":\"%lu\",\"result\":\"%s\",\"channel\":%u,\"appliedState\":\"%s\",\"sequence\":%lu}",
		static_cast<unsigned long>(command.correlationId), trackedStatusText(command.status), command.channel.value,
		stateText(command.appliedState), static_cast<unsigned long>(command.resourceSequence));
	return sendJson(response, status, body);
}

esp_err_t WebServerAdapter::getCommandResult(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::RelayRead, false, authorization)) return sendUnauthorized(response);
	if (dependencies_.commandTracker == nullptr) return sendUnavailable(response, "command.unavailable");
	const auto path = boundedText(request->pathCStr(), 96);
	constexpr std::string_view prefix{"/api/v1/commands/"};
	unsigned long correlationId{};
	char trailing{};
	if (path.size() <= prefix.size() || path.substr(0, prefix.size()) != prefix ||
		std::sscanf(path.data() + prefix.size(), "%lu%c", &correlationId, &trailing) != 1)
		return sendJson(response, 404, "{\"error\":{\"code\":\"command.not_found\",\"message\":\"Command result not found.\"}}");
	app::WebTrackedCommand tracked{};
	if (!dependencies_.commandTracker->findByCorrelation(authorization.sessionId,
		static_cast<std::uint32_t>(correlationId), tracked))
		return sendJson(response, 404, "{\"error\":{\"code\":\"command.not_found\",\"message\":\"Command result not found.\"}}");
	return sendCommandResult(response, tracked);
}

esp_err_t WebServerAdapter::enqueueOperation(PsychicResponse *response,
	const app::WebApplicationRequest &operation) noexcept
{
	if (dependencies_.requestQueue == nullptr || operation.operationId == 0 ||
		!dependencies_.requestQueue->enqueue(operation))
		return sendJson(response, 429, "{\"error\":{\"code\":\"web.request_queue_full\",\"message\":\"Request queue is full.\"}}");
	char body[160]{};
	std::snprintf(body, sizeof(body), "{\"operationId\":\"%lu\",\"status\":\"pending\"}",
		static_cast<unsigned long>(operation.operationId));
	return sendJson(response, 202, body);
}

esp_err_t WebServerAdapter::getOperationResult(PsychicRequest *request, PsychicResponse *response) noexcept
{
	ports::WebAuthorization authorization{};
	if (!authorize(request, ports::WebPermission::ConfigurationRead, false, authorization)) return sendUnauthorized(response);
	if (dependencies_.requestQueue == nullptr) return sendUnavailable(response, "operation.unavailable");
	const auto path = boundedText(request->pathCStr(), 96);
	constexpr std::string_view prefix{"/api/v1/operations/"};
	unsigned long operationId{};
	char trailing{};
	if (path.size() <= prefix.size() || path.substr(0, prefix.size()) != prefix ||
		std::sscanf(path.data() + prefix.size(), "%lu%c", &operationId, &trailing) != 1)
		return sendJson(response, 404, "{\"error\":{\"code\":\"operation.not_found\",\"message\":\"Operation not found.\"}}");
	app::WebOperationResult result{};
	if (!dependencies_.requestQueue->findResult(authorization.sessionId, static_cast<std::uint32_t>(operationId), result))
		return sendJson(response, 404, "{\"error\":{\"code\":\"operation.not_found\",\"message\":\"Operation not found.\"}}");
	const auto *status = result.status == app::WebOperationStatus::Pending ? "pending" :
		result.status == app::WebOperationStatus::Applied ? "applied" :
		result.status == app::WebOperationStatus::Conflict ? "conflict" :
		result.status == app::WebOperationStatus::Invalid ? "invalid" :
		result.status == app::WebOperationStatus::Rejected ? "rejected" : "unavailable";
	char body[160]{};
	std::snprintf(body, sizeof(body), "{\"operationId\":\"%lu\",\"status\":\"%s\"}", operationId, status);
	return sendJson(response, 200, body);
}

esp_err_t WebServerAdapter::sendStatic(PsychicRequest *, PsychicResponse *response, const char *path, const char *contentType, const char *cacheControl) const noexcept
{
	if (!LittleFS.exists(path)) return sendUnavailable(response, "assets.unavailable");
	addSecurityHeaders(response);
	response->addHeader("Cache-Control", cacheControl);
	PsychicFileResponse file{response, LittleFS, String(path), String(contentType)};
	return file.send();
}

esp_err_t WebServerAdapter::sendManifestAsset(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	const auto path = boundedText(request->pathCStr(), 160);
	const auto asset = std::find_if(staticAssets_.begin(), staticAssets_.begin() + staticAssetCount_, [&path](const auto &entry) {
		return path == boundedText(entry.url.data(), entry.url.size());
	});
	if (asset == staticAssets_.begin() + staticAssetCount_)
		return sendJson(response, 404, "{\"error\":{\"code\":\"asset.not_found\",\"message\":\"Asset not found.\"}}");
	const auto *acceptEncoding = request->headerCStr("Accept-Encoding");
	const auto useGzip = acceptEncoding != nullptr && std::strstr(acceptEncoding, "gzip") != nullptr &&
		LittleFS.exists(asset->gzipFile.data());
	if (useGzip) response->addHeader("Content-Encoding", "gzip");
	response->addHeader("Vary", "Accept-Encoding");
	return sendStatic(request, response, useGzip ? asset->gzipFile.data() : asset->file.data(),
		asset->contentType.data(), "public, max-age=31536000, immutable");
}

esp_err_t WebServerAdapter::sendIndex(PsychicRequest *request, PsychicResponse *response) const noexcept
{
	if (LittleFS.exists(indexPath)) return sendStatic(request, response, indexPath, "text/html; charset=utf-8", "no-cache");
	addSecurityHeaders(response);
	return response->send(503, "text/html; charset=utf-8", "<!doctype html><html><head><meta charset=utf-8><title>Switch actuator</title></head><body><main><h1>Management interface unavailable</h1><p>The embedded filesystem could not be loaded. Relay operation remains active.</p></main></body></html>");
}

esp_err_t WebServerAdapter::sendUnauthorized(PsychicResponse *response) const noexcept
{
	return sendJson(response, 401, "{\"error\":{\"code\":\"session.unauthorized\",\"message\":\"Authentication required.\"}}");
}

esp_err_t WebServerAdapter::sendUnavailable(PsychicResponse *response, const char *code) const noexcept
{
	char body[192]{};
	std::snprintf(body, sizeof(body), "{\"error\":{\"code\":\"%s\",\"message\":\"Service unavailable.\"}}", code);
	return sendJson(response, 503, body);
}

void WebServerAdapter::onWebSocketOpen(PsychicWebSocketClient *client) noexcept
{
	if (client == nullptr) return;
	const auto pending = std::find_if(pendingAuthorizations_.begin(), pendingAuthorizations_.end(),
		[client](const auto &entry) { return entry.socket == client->socket(); });
	if (pending == pendingAuthorizations_.end()) { static_cast<void>(client->close()); return; }
	const auto empty = std::find_if(clients_.begin(), clients_.end(), [](const auto &state) { return state.client == nullptr; });
	if (empty == clients_.end()) { *pending = {}; static_cast<void>(client->close()); return; }
	const auto nowMs = millis();
	*empty = {client, pending->sessionId, pending->permissions,
		dependencies_.events != nullptr ? dependencies_.events->latestSequence() + 1U : 1U, nowMs, nowMs, 0};
	*pending = {};
	char message[160]{};
	std::snprintf(message, sizeof(message), "{\"version\":1,\"type\":\"session.ready\",\"sequence\":0,\"payload\":{\"bootId\":\"%s\"}}", bootId_.data());
	static_cast<void>(client->sendMessage(message));
}

void WebServerAdapter::onWebSocketClose(PsychicWebSocketClient *client) noexcept
{
	if (auto *state = findClient(client); state != nullptr) *state = {};
}

esp_err_t WebServerAdapter::onWebSocketFrame(PsychicWebSocketRequest *request, httpd_ws_frame *frame) noexcept
{
	auto *state = request != nullptr ? findClient(request->client()) : nullptr;
	if (state == nullptr || frame == nullptr || frame->type != HTTPD_WS_TYPE_TEXT || !frame->final || frame->len == 0 || frame->len > maximumFrameBytes)
	{
		if (request != nullptr) static_cast<void>(request->client()->close());
		return ESP_ERR_INVALID_ARG;
	}
	const auto nowMs = millis();
	state->lastActivityAtMs = nowMs;
	if (nowMs - state->messageWindowStartedAtMs >= 1000U)
	{
		state->messageWindowStartedAtMs = nowMs;
		state->messagesInWindow = 0;
	}
	if (state->messagesInWindow >= 12U)
	{
		static_cast<void>(request->client()->close());
		return ESP_ERR_INVALID_STATE;
	}
	++state->messagesInWindow;
	JsonDocument document;
	if (deserializeJson(document, frame->payload, frame->len) != DeserializationError::Ok)
		return request->reply("{\"version\":1,\"type\":\"protocol.error\",\"sequence\":0,\"payload\":{\"code\":\"protocol.invalid_message\"}}");
	if (document["version"].as<int>() != 1)
		return request->reply("{\"version\":1,\"type\":\"protocol.error\",\"sequence\":0,\"payload\":{\"code\":\"protocol.version_mismatch\"}}");
	const auto type = document["type"].as<const char *>();
	if (type != nullptr && std::strcmp(type, "ping") == 0)
		return request->reply("{\"version\":1,\"type\":\"pong\",\"sequence\":0,\"payload\":{}}");
	if (type != nullptr && std::strcmp(type, "relay.command") == 0 &&
		(state->permissions & static_cast<std::uint32_t>(ports::WebPermission::RelayCommand)) != 0 &&
		dependencies_.switchingPolicy != nullptr && dependencies_.commandTracker != nullptr)
	{
		const auto channel = document["payload"]["channel"].as<std::uint8_t>();
		const auto expectedSequence = document["payload"]["expectedSequence"].as<std::uint32_t>();
		const auto *requestId = document["requestId"].as<const char *>();
		domain::RelayAction action{};
		if (channel < domain::relayChannelCount && requestId != nullptr &&
			document["payload"]["expectedSequence"].is<std::uint32_t>() &&
			parseRelayAction(document["payload"]["action"], action))
		{
			const auto correlationId = esp_random();
			app::WebTrackedCommand tracked{};
			const auto beginResult = dependencies_.commandTracker->begin(state->sessionId, requestId, {channel}, action,
				expectedSequence, correlationId, nowMs, tracked);
			if (beginResult == app::WebCommandBeginResult::IdempotencyMismatch)
				return request->reply("{\"version\":1,\"type\":\"protocol.error\",\"sequence\":0,\"payload\":{\"code\":\"web.idempotency_mismatch\"}}");
			if (beginResult == app::WebCommandBeginResult::CapacityFull)
				return request->reply("{\"version\":1,\"type\":\"protocol.error\",\"sequence\":0,\"payload\":{\"code\":\"web.command_capacity\"}}");
			if (beginResult == app::WebCommandBeginResult::Accepted)
			{
				app::WebApplicationRequest operation{};
				operation.type = app::WebRequestType::RelayCommand;
				operation.operationId = correlationId;
				operation.sessionId = state->sessionId;
				operation.correlationId = correlationId;
				operation.channel = {channel};
				operation.action = action;
				operation.receivedAtMs = nowMs;
				if (dependencies_.requestQueue == nullptr || !dependencies_.requestQueue->enqueue(operation))
					static_cast<void>(dependencies_.commandTracker->reject(correlationId, app::RelayCommandReason::EventRejected,
						nowMs, tracked));
			}
			else if (beginResult != app::WebCommandBeginResult::Duplicate)
			{
				return request->reply("{\"version\":1,\"type\":\"protocol.error\",\"sequence\":0,\"payload\":{\"code\":\"protocol.invalid_message\"}}");
			}
			char message[320]{};
			std::snprintf(message, sizeof(message), "{\"version\":1,\"type\":\"relay.commandResult\",\"sequence\":0,\"requestId\":\"%s\",\"payload\":{\"correlationId\":\"%lu\",\"channel\":%u,\"result\":\"%s\"}}",
				requestId, static_cast<unsigned long>(tracked.correlationId), channel, trackedStatusText(tracked.status));
			return request->reply(message);
		}
	}
	return request->reply("{\"version\":1,\"type\":\"protocol.error\",\"sequence\":0,\"payload\":{\"code\":\"protocol.unsupported_message\"}}");
}

void WebServerAdapter::publishEvents(const std::uint32_t nowMs) noexcept
{
	if (dependencies_.events == nullptr) return;
	for (auto &state : clients_)
	{
		if (state.client == nullptr) continue;
		if (nowMs - state.lastActivityAtMs >= clientIdleTimeoutMs) { static_cast<void>(state.client->close()); state = {}; continue; }
		app::WebEvent event{};
		const auto readResult = dependencies_.events->read(state.nextEventSequence, event);
		if (readResult == app::WebEventReadResult::Gap)
		{
			if (webSocketSequenceGaps_ != UINT32_MAX) ++webSocketSequenceGaps_;
			if (state.client->sendMessage("{\"version\":1,\"type\":\"resync.required\",\"sequence\":0,\"payload\":{}}") != ESP_OK)
			{
				if (webSocketSendFailures_ != UINT32_MAX) ++webSocketSendFailures_;
				static_cast<void>(state.client->close());
				state = {};
				continue;
			}
			state.nextEventSequence = dependencies_.events->latestSequence() + 1U;
		}
		else if (readResult == app::WebEventReadResult::Available)
		{
			char message[512]{};
			if (event.type == app::WebEventType::RelayCommandCompleted || event.type == app::WebEventType::RelayStateChanged)
			{
				std::snprintf(message, sizeof(message), "{\"version\":1,\"type\":\"relay.commandResult\",\"sequence\":%lu,\"payload\":{\"bootId\":\"%s\",\"resource\":\"relay:%u\",\"resourceSequence\":%lu,\"correlationId\":\"%lu\",\"channel\":%u,\"appliedState\":\"%s\",\"result\":\"%s\"}}", static_cast<unsigned long>(event.sequence), bootId_.data(), event.channel.value, static_cast<unsigned long>(event.resourceSequence), static_cast<unsigned long>(event.correlationId), event.channel.value, stateText(event.appliedState), event.commandStatus == app::RelayCommandStatus::Accepted ? "applied" : "rejected");
			}
			else
			{
				const auto *type = event.type == app::WebEventType::NetworkChanged ? "network.stateChanged" :
					event.type == app::WebEventType::WifiScanStarted ? "wifi.scanStarted" :
					event.type == app::WebEventType::WifiScanCompleted ? "wifi.scanCompleted" :
					event.type == app::WebEventType::ConfigurationChanged ? "configuration.changed" : "diagnostics.changed";
				const auto *resource = event.type == app::WebEventType::NetworkChanged ? "network" :
					(event.type == app::WebEventType::WifiScanStarted || event.type == app::WebEventType::WifiScanCompleted) ?
					"wifi.scan" : event.type == app::WebEventType::ConfigurationChanged ? "configuration" : "diagnostics";
				std::snprintf(message, sizeof(message),
					"{\"version\":1,\"type\":\"%s\",\"sequence\":%lu,\"payload\":{\"bootId\":\"%s\",\"resource\":\"%s\",\"resourceSequence\":%lu}}",
					type, static_cast<unsigned long>(event.sequence), bootId_.data(), resource,
					static_cast<unsigned long>(event.resourceSequence));
			}
			if (state.client->sendMessage(message) != ESP_OK)
			{
				if (webSocketSendFailures_ != UINT32_MAX) ++webSocketSendFailures_;
				static_cast<void>(state.client->close());
				state = {};
				continue;
			}
			++state.nextEventSequence;
		}
	}
}

WebServerAdapter::ClientState *WebServerAdapter::findClient(PsychicWebSocketClient *client) noexcept
{
	const auto match = std::find_if(clients_.begin(), clients_.end(), [client](const auto &state) { return state.client == client; });
	return match == clients_.end() ? nullptr : &*match;
}
}
