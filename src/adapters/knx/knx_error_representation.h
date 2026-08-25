#pragma once

#include "../../domain/error.h"

#include <cstdint>

namespace switch_actuator::adapters::knx
{
enum class KnxErrorRepresentation : std::uint8_t
{
	SilentReject,
	SilentRejectBusy
};

[[nodiscard]] constexpr KnxErrorRepresentation represent(const domain::ErrorCode error) noexcept
{
	return error == domain::ErrorCode::Busy ? KnxErrorRepresentation::SilentRejectBusy :
		KnxErrorRepresentation::SilentReject;
}
}