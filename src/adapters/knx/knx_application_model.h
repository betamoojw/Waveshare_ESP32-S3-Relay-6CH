#pragma once

#include "../../domain/relay_types.h"
#include "../../domain/version_compatibility.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::adapters::knx
{
inline constexpr std::uint8_t applicationModelMajorVersion{
	static_cast<std::uint8_t>(domain::compatibility::knxApplication.major)};
inline constexpr std::uint8_t applicationModelMinorVersion{0};
inline constexpr std::uint16_t invalidCommunicationObjectNumber{0xFFFF};
inline constexpr std::uint16_t firstChannelObjectNumber{16};
inline constexpr std::uint16_t channelObjectBlockSize{16};

enum class DatapointType : std::uint8_t
{
	Switch1_001,
	Boolean1_002,
	Enable1_003,
	Alarm1_005,
	SceneNumber17_001,
	SceneControl18_001
};

enum class CommunicationObjectFlag : std::uint8_t
{
	Communication = 1U << 0U,
	Read = 1U << 1U,
	Write = 1U << 2U,
	Transmit = 1U << 3U,
	Update = 1U << 4U
};

using CommunicationObjectFlags = std::uint8_t;

[[nodiscard]] constexpr CommunicationObjectFlags operator|(const CommunicationObjectFlag left,
															const CommunicationObjectFlag right) noexcept
{
	return static_cast<CommunicationObjectFlags>(left) | static_cast<CommunicationObjectFlags>(right);
}

enum class CommunicationObjectExposure : std::uint8_t
{
	Conditional,
	ReservedNotImplemented
};

enum class CommunicationObjectRole : std::uint8_t
{
	Heartbeat,
	CentralSwitch,
	CentralOff,
	DeviceFault,
	ChannelSwitch,
	ChannelStatus,
	ChannelFault,
	SceneRecall,
	SceneControl
};

struct CommunicationObjectDescriptor final
{
	std::uint16_t number;
	std::string_view name;
	CommunicationObjectRole role;
	DatapointType datapointType;
	CommunicationObjectFlags flags;
	CommunicationObjectExposure exposure;
};

inline constexpr CommunicationObjectFlags communicationWrite =
	CommunicationObjectFlag::Communication | CommunicationObjectFlag::Write;
inline constexpr CommunicationObjectFlags communicationTransmit =
	CommunicationObjectFlag::Communication | CommunicationObjectFlag::Transmit;
inline constexpr CommunicationObjectFlags communicationReadTransmit =
	CommunicationObjectFlag::Communication | CommunicationObjectFlag::Read |
	static_cast<CommunicationObjectFlags>(CommunicationObjectFlag::Transmit);

inline constexpr std::array<CommunicationObjectDescriptor, 4> deviceCommunicationObjects{{
	{0, "Device in operation", CommunicationObjectRole::Heartbeat, DatapointType::Boolean1_002,
		communicationTransmit, CommunicationObjectExposure::Conditional},
	{1, "Central switch", CommunicationObjectRole::CentralSwitch, DatapointType::Switch1_001,
		communicationWrite, CommunicationObjectExposure::Conditional},
	{2, "Central off", CommunicationObjectRole::CentralOff, DatapointType::Enable1_003,
		communicationWrite, CommunicationObjectExposure::Conditional},
	{4, "Device fault", CommunicationObjectRole::DeviceFault, DatapointType::Alarm1_005,
		communicationTransmit, CommunicationObjectExposure::Conditional},
}};

inline constexpr std::array<CommunicationObjectDescriptor, 5> channelCommunicationObjectTemplates{{
	{0, "Switch", CommunicationObjectRole::ChannelSwitch, DatapointType::Switch1_001,
		communicationWrite, CommunicationObjectExposure::Conditional},
	{1, "Applied state", CommunicationObjectRole::ChannelStatus, DatapointType::Switch1_001,
		communicationTransmit, CommunicationObjectExposure::Conditional},
	{4, "Scene recall", CommunicationObjectRole::SceneRecall, DatapointType::SceneNumber17_001,
		0, CommunicationObjectExposure::ReservedNotImplemented},
	{5, "Scene control", CommunicationObjectRole::SceneControl, DatapointType::SceneControl18_001,
		0, CommunicationObjectExposure::ReservedNotImplemented},
	{9, "Fault", CommunicationObjectRole::ChannelFault, DatapointType::Alarm1_005,
		communicationTransmit, CommunicationObjectExposure::Conditional},
}};

[[nodiscard]] constexpr bool hasFlag(const CommunicationObjectFlags flags,
											 const CommunicationObjectFlag flag) noexcept
{
	return (flags & static_cast<CommunicationObjectFlags>(flag)) != 0;
}

[[nodiscard]] constexpr CommunicationObjectFlags channelSwitchFlags(const bool readable) noexcept
{
	return readable ? communicationWrite | static_cast<CommunicationObjectFlags>(CommunicationObjectFlag::Read)
		: communicationWrite;
}

[[nodiscard]] constexpr bool isExposedInVersion1(const CommunicationObjectDescriptor &descriptor) noexcept
{
	return descriptor.exposure == CommunicationObjectExposure::Conditional;
}

[[nodiscard]] constexpr std::uint16_t channelObjectNumber(const std::size_t channel,
														 const std::uint16_t offset) noexcept
{
	return channel < domain::relayChannelCount && offset < channelObjectBlockSize
		? static_cast<std::uint16_t>(firstChannelObjectNumber + channel * channelObjectBlockSize + offset)
		: invalidCommunicationObjectNumber;
}

[[nodiscard]] constexpr std::uint16_t packIndividualAddress(const std::uint8_t area,
														 const std::uint8_t line,
														 const std::uint8_t device) noexcept
{
	return area <= 15 && line <= 15
		? static_cast<std::uint16_t>((static_cast<std::uint16_t>(area) << 12U) |
			(static_cast<std::uint16_t>(line) << 8U) | device)
		: std::uint16_t{0};
}

[[nodiscard]] constexpr std::uint16_t packThreeLevelGroupAddress(const std::uint8_t main,
															 const std::uint8_t middle,
															 const std::uint8_t sub) noexcept
{
	return main <= 31 && middle <= 7
		? static_cast<std::uint16_t>((static_cast<std::uint16_t>(main) << 11U) |
			(static_cast<std::uint16_t>(middle) << 8U) | sub)
		: std::uint16_t{0};
}

[[nodiscard]] constexpr bool isConfiguredGroupAddress(const std::uint16_t address) noexcept
{
	return address != 0;
}

[[nodiscard]] constexpr std::string_view datapointTypeName(const DatapointType type) noexcept
{
	switch (type)
	{
	case DatapointType::Switch1_001: return "DPT 1.001";
	case DatapointType::Boolean1_002: return "DPT 1.002";
	case DatapointType::Enable1_003: return "DPT 1.003";
	case DatapointType::Alarm1_005: return "DPT 1.005";
	case DatapointType::SceneNumber17_001: return "DPT 17.001";
	case DatapointType::SceneControl18_001: return "DPT 18.001";
	default: return "unknown";
	}
}
}
