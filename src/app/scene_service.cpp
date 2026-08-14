#include "scene_service.h"

namespace switch_actuator::app
{
SceneService::SceneService(SwitchingPolicyService &switchingPolicy,
											 const RelayCommandService &relayService) noexcept
	: switchingPolicy_{switchingPolicy}, relayService_{relayService}
{
}

SceneConfigureResult SceneService::configure(const SceneDefinition *const scenes,
																		 const std::size_t sceneCount,
																		 const ScenePersistencePort persistence) noexcept
{
	disable();
	if (sceneCount == 0 || scenes == nullptr || !persistence.isValid())
	{
		return SceneConfigureResult::Disabled;
	}
	if (sceneCount > scenes_.size())
	{
		return SceneConfigureResult::TooManyScenes;
	}
	for (std::size_t index = 0; index < sceneCount; ++index)
	{
		if (!isValid(scenes[index]))
		{
			return SceneConfigureResult::InvalidScene;
		}
		for (std::size_t previous = 0; previous < index; ++previous)
		{
			if (scenes[previous].number == scenes[index].number)
			{
				return SceneConfigureResult::DuplicateScene;
			}
		}
	}
	for (std::size_t index = 0; index < sceneCount; ++index)
	{
		scenes_[index] = scenes[index];
	}
	sceneCount_ = sceneCount;
	persistence_ = persistence;
	enabled_ = true;
	return SceneConfigureResult::Configured;
}

SceneOperationResult SceneService::recall(const std::uint8_t sceneNumber,
																			const domain::CommandSource source,
																			const std::uint32_t correlationId,
																			const std::uint32_t nowMs) noexcept
{
	if (!enabled_)
	{
		return SceneOperationResult::Disabled;
	}
	const auto *const scene = find(sceneNumber);
	if (scene == nullptr)
	{
		return SceneOperationResult::UnknownScene;
	}
	const auto result = switchingPolicy_.requestStates(
		scene->participants, scene->targetStates, source, correlationId, nowMs);
	if (result == SwitchingPolicyResult::NoParticipants)
	{
		return SceneOperationResult::NoParticipants;
	}
	return result == SwitchingPolicyResult::Accepted ? SceneOperationResult::Accepted :
		SceneOperationResult::PolicyRejected;
}

SceneOperationResult SceneService::learn(const std::uint8_t sceneNumber) noexcept
{
	if (!enabled_)
	{
		return SceneOperationResult::Disabled;
	}
	auto *const scene = find(sceneNumber);
	if (scene == nullptr)
	{
		return SceneOperationResult::UnknownScene;
	}
	auto learned = *scene;
	const auto &snapshots = relayService_.snapshots();
	for (std::size_t channel = 0; channel < snapshots.size(); ++channel)
	{
		if (learned.participants[channel])
		{
			learned.targetStates[channel] = snapshots[channel].appliedState;
		}
	}
	if (!persistence_.persist(learned, persistence_.context))
	{
		return SceneOperationResult::PersistenceFailure;
	}
	*scene = learned;
	return SceneOperationResult::Accepted;
}

bool SceneService::isEnabled() const noexcept
{
	return enabled_;
}

void SceneService::disable() noexcept
{
	scenes_.fill({});
	persistence_ = {};
	sceneCount_ = 0;
	enabled_ = false;
}

bool SceneService::isValid(const SceneDefinition &scene) noexcept
{
	if (scene.number == 0 || scene.number > 64)
	{
		return false;
	}
	for (const auto participates : scene.participants)
	{
		if (participates)
		{
			return true;
		}
	}
	return false;
}

SceneDefinition *SceneService::find(const std::uint8_t sceneNumber) noexcept
{
	for (std::size_t index = 0; index < sceneCount_; ++index)
	{
		if (scenes_[index].number == sceneNumber)
		{
			return &scenes_[index];
		}
	}
	return nullptr;
}
}