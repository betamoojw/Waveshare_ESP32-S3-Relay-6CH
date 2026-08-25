#pragma once

#include "web_api_v1_contract.h"
#include "web_error_representation.h"

#include "../../app/configuration_service.h"
#include "../../app/diagnostics_service.h"
#include "../../app/relay_command_service.h"
#include "../../app/switching_policy_service.h"
#include "../../app/web_event_journal.h"
#include "../../app/web_security_service.h"
#include "../../app/web_command_tracker.h"
#include "../../app/web_request_queue.h"
#include "../../app/wifi_management_service.h"
#include "../../ports/modbus_rtu_control_port.h"
#include "../../ports/network_control_port.h"
#include "../../ports/network_status_port.h"
#include "../../ports/web_security_port.h"

#ifdef HTTP_ANY
#undef HTTP_ANY
#endif
#define HTTPAuthMethod PsychicHttpAuthMethod
#define BASIC_AUTH PSYCHIC_BASIC_AUTH
#define DIGEST_AUTH PSYCHIC_DIGEST_AUTH
#include <PsychicHttpsServer.h>
#include <PsychicWebSocket.h>
#undef DIGEST_AUTH
#undef BASIC_AUTH
#undef HTTPAuthMethod

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::adapters::web
{
enum class WebServerInitializeResult : std::uint8_t
{
	Initialized,
	Disabled,
	SecurityUnavailable,
	CertificateUnavailable,
	StartFailure
};

struct WebServerDependencies final
{
	const app::RelayCommandService *relayService{nullptr};
	app::SwitchingPolicyService *switchingPolicy{nullptr};
	const app::DiagnosticsService *diagnostics{nullptr};
	const app::ConfigurationService *configuration{nullptr};
	app::WifiManagementService *wifiManagement{nullptr};
	const app::WebEventJournal *events{nullptr};
	ports::NetworkStatusPort networkStatus{};
	ports::NetworkControlPort networkControl{};
	ports::ModbusRtuControlPort modbusControl{};
	ports::WebSecurityPort security{};
	app::WebSecurityService *securityService{nullptr};
	app::WebCommandTracker *commandTracker{nullptr};
	app::WebRequestQueue *requestQueue{nullptr};
};

class WebServerAdapter final
{
public:
	explicit WebServerAdapter(WebServerDependencies dependencies) noexcept;

	[[nodiscard]] WebServerInitializeResult initialize() noexcept;
	void update(std::uint32_t nowMs) noexcept;
	void stop() noexcept;
	[[nodiscard]] bool isRunning() const noexcept;

private:
	static constexpr std::size_t maximumWebSocketClients{api_v1::maximumWebSocketClients};
	static constexpr std::size_t maximumFrameBytes{api_v1::maximumRequestBodyBytes};
	static constexpr std::size_t maximumStaticAssets{16};
	static constexpr std::size_t reservedHttpSockets{3};
	static constexpr std::size_t maximumOpenSockets{maximumWebSocketClients + reservedHttpSockets};
	static constexpr std::size_t httpsTaskStackBytes{8192};
	static constexpr std::uint32_t clientIdleTimeoutMs{api_v1::webSocketIdleTimeoutMs};
	static constexpr std::uint32_t pendingAuthorizationTimeoutMs{10'000};

	struct ClientState final
	{
		PsychicWebSocketClient *client{nullptr};
		std::uint32_t sessionId{0};
		std::uint32_t permissions{0};
		std::uint32_t nextEventSequence{1};
		std::uint32_t lastActivityAtMs{0};
		std::uint32_t messageWindowStartedAtMs{0};
		std::uint8_t messagesInWindow{0};
	};
	struct PendingAuthorization final
	{
		int socket{-1};
		std::uint32_t sessionId{0};
		std::uint32_t permissions{0};
		std::uint32_t authorizedAtMs{0};
	};
	struct StaticAsset final
	{
		std::array<char, 128> url{};
		std::array<char, 160> file{};
		std::array<char, 164> gzipFile{};
		std::array<char, 48> contentType{};
	};

	void registerRoutes() noexcept;
	[[nodiscard]] bool loadStaticAssetManifest() noexcept;
	[[nodiscard]] ports::WebAuthorizationResult authorize(PsychicRequest *request,
		ports::WebPermission permission,
		bool mutation,
		ports::WebAuthorization &authorization) const noexcept;
	[[nodiscard]] bool authorizeWebSocket(PsychicRequest *request) noexcept;
	[[nodiscard]] esp_err_t createSession(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t sendSession(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t deleteSession(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t sendSessionView(PsychicResponse *response, const app::WebSessionView &view) const noexcept;
	[[nodiscard]] esp_err_t sendCapabilities(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendDevice(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendDiagnostics(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendNetwork(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendRelays(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendWifi(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendModbusConfiguration(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t saveModbusConfiguration(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t setModbusRole(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t sendKnxConfiguration(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t saveKnxConfiguration(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t sendUsers(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t saveUser(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t requestRestart(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t rejectFactoryReset(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t rejectOta(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t startWifiScan(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t saveWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t removeWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t moveWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t connectWifiProfile(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t saveRecoveryAp(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t submitRelayCommand(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t sendCommandResult(PsychicResponse *response,
		const app::WebTrackedCommand &command,
		int status = 200) const noexcept;
	[[nodiscard]] esp_err_t getCommandResult(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t enqueueOperation(PsychicResponse *response,
		const app::WebApplicationRequest &operation) noexcept;
	[[nodiscard]] esp_err_t getOperationResult(PsychicRequest *request, PsychicResponse *response) noexcept;
	[[nodiscard]] esp_err_t sendWifiMutationResult(PsychicRequest *request,
		PsychicResponse *response,
		app::WifiManagementResult result) noexcept;
	[[nodiscard]] esp_err_t sendStatic(PsychicRequest *request,
		PsychicResponse *response,
		const char *path,
		const char *contentType,
		const char *cacheControl) const noexcept;
	[[nodiscard]] esp_err_t sendManifestAsset(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendIndex(PsychicRequest *request, PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendUnauthorized(PsychicResponse *response) const noexcept;
	[[nodiscard]] esp_err_t sendAuthorizationFailure(PsychicResponse *response,
		ports::WebAuthorizationResult result) const noexcept;
	[[nodiscard]] esp_err_t sendError(PsychicResponse *response, domain::ErrorCode error) const noexcept;
	void onWebSocketOpen(PsychicWebSocketClient *client) noexcept;
	void onWebSocketClose(PsychicWebSocketClient *client) noexcept;
	[[nodiscard]] esp_err_t onWebSocketFrame(PsychicWebSocketRequest *request, httpd_ws_frame *frame) noexcept;
	void publishEvents(std::uint32_t nowMs) noexcept;
	[[nodiscard]] ClientState *findClient(PsychicWebSocketClient *client) noexcept;

	WebServerDependencies dependencies_;
	PsychicHttpsServer server_{443};
	PsychicWebSocketHandler webSocket_{};
	std::array<ClientState, maximumWebSocketClients> clients_{};
	std::array<PendingAuthorization, maximumWebSocketClients> pendingAuthorizations_{};
	std::array<StaticAsset, maximumStaticAssets> staticAssets_{};
	std::size_t staticAssetCount_{0};
	std::array<char, 33> bootId_{};
	std::uint32_t webSocketSendFailures_{0};
	std::uint32_t webSocketSequenceGaps_{0};
	bool routesRegistered_{false};
	bool running_{false};
};
}