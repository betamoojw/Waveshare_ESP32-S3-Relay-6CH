#pragma once

#include <cstdint>

namespace switch_actuator::domain
{
struct PersistentDiagnosticCounters final
{
	std::uint32_t bootCount{0};
	std::uint32_t watchdogCount{0};
	std::uint32_t brownoutCount{0};
	std::uint32_t configErrorCount{0};
	std::uint32_t otaFailureCount{0};
	std::uint32_t networkFailureCount{0};
	std::uint32_t modbusErrorCount{0};
	std::uint32_t knxErrorCount{0};
	std::uint32_t storageErrorCount{0};
};
}