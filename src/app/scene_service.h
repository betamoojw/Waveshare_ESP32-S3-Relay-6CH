#pragma once

#include "relay_command_service.h"
#include "switching_policy_service.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::app
{
inline constexpr std::size_t maximumSceneCount{16};

struct SceneDefinition final
{
	std::uint8_t number{0};
	std::array<bool, domain::relayChannelCount> participants{};
	std::array<domain::RelayState, domain::relayChannelCount> targetStates{};
};

using PersistSceneHandler = bool (*)(const SceneDefinition &scene, void *context) noexcept;

struct ScenePersistencePort final
{
	PersistSceneHandler persist{nullptr};
	void *context{nullptr};

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return persist != nullptr;
	}
};

enum class SceneConfigureResult : std::uint8_t
{
	Configured,
	Disabled,
	TooManyScenes,
	InvalidScene,
	DuplicateScene
};

enum class SceneOperationResult : std::uint8_t
{
	Accepted,
	Disabled,
	UnknownScene,
	NoParticipants,
	PersistenceFailure,
	PolicyRejected
};

class SceneService final
{
public:
	SceneService(SwitchingPolicyService &switchingPolicy, const RelayCommandService &relayService) noexcept;

	[[nodiscard]] SceneConfigureResult configure(const SceneDefinition *scenes,
															 std::size_t sceneCount,
															 ScenePersistencePort persistence) noexcept;
	[[nodiscard]] SceneOperationResult recall(std::uint8_t sceneNumber,
													  domain::CommandSource source,
													  std::uint32_t correlationId,
													  std::uint32_t nowMs) noexcept;
	[[nodiscard]] SceneOperationResult learn(std::uint8_t sceneNumber) noexcept;
	[[nodiscard]] bool isEnabled() const noexcept;
	void disable() noexcept;

private:
	[[nodiscard]] static bool isValid(const SceneDefinition &scene) noexcept;
	[[nodiscard]] SceneDefinition *find(std::uint8_t sceneNumber) noexcept;

	SwitchingPolicyService &switchingPolicy_;
	const RelayCommandService &relayService_;
	std::array<SceneDefinition, maximumSceneCount> scenes_{};
	ScenePersistencePort persistence_{};
	std::size_t sceneCount_{0};
	bool enabled_{false};
};
}