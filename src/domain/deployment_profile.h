#pragma once

#include <cstdint>
#include <string_view>

namespace switch_actuator::domain
{
enum class DeploymentProfile : std::uint8_t
{
	Development = 0,
	Engineering = 1,
	Production = 2
};

#ifndef SWITCH_ACTUATOR_DEPLOYMENT_PROFILE
#define SWITCH_ACTUATOR_DEPLOYMENT_PROFILE 0
#endif

#ifndef SWITCH_ACTUATOR_ENABLE_DEBUG_INTERFACES
#define SWITCH_ACTUATOR_ENABLE_DEBUG_INTERFACES 1
#endif

#ifndef SWITCH_ACTUATOR_ALLOW_DEVELOPMENT_CREDENTIALS
#define SWITCH_ACTUATOR_ALLOW_DEVELOPMENT_CREDENTIALS 1
#endif

#ifndef SWITCH_ACTUATOR_REQUIRE_SECURE_BOOT
#define SWITCH_ACTUATOR_REQUIRE_SECURE_BOOT 0
#endif

#ifndef SWITCH_ACTUATOR_REQUIRE_FLASH_ENCRYPTION
#define SWITCH_ACTUATOR_REQUIRE_FLASH_ENCRYPTION 0
#endif

#ifndef SWITCH_ACTUATOR_REQUIRE_SIGNED_FIRMWARE
#define SWITCH_ACTUATOR_REQUIRE_SIGNED_FIRMWARE 0
#endif

static_assert(SWITCH_ACTUATOR_DEPLOYMENT_PROFILE >= 0 && SWITCH_ACTUATOR_DEPLOYMENT_PROFILE <= 2,
	"SWITCH_ACTUATOR_DEPLOYMENT_PROFILE must be 0 (development), 1 (engineering), or 2 (production)");

#if SWITCH_ACTUATOR_DEPLOYMENT_PROFILE == 2
#ifndef NDEBUG
#error "Production builds require NDEBUG and release optimization"
#endif
static_assert(SWITCH_ACTUATOR_ENABLE_DEBUG_INTERFACES == 0,
	"Production builds must disable debug interfaces");
static_assert(SWITCH_ACTUATOR_ALLOW_DEVELOPMENT_CREDENTIALS == 0,
	"Production builds must reject development credentials");
static_assert(SWITCH_ACTUATOR_REQUIRE_SECURE_BOOT == 1,
	"Production builds must require secure boot");
static_assert(SWITCH_ACTUATOR_REQUIRE_FLASH_ENCRYPTION == 1,
	"Production builds must require flash encryption");
static_assert(SWITCH_ACTUATOR_REQUIRE_SIGNED_FIRMWARE == 1,
	"Production builds must require signed firmware");
#endif

inline constexpr auto deploymentProfile = static_cast<DeploymentProfile>(SWITCH_ACTUATOR_DEPLOYMENT_PROFILE);

[[nodiscard]] constexpr std::string_view deploymentProfileName(const DeploymentProfile profile) noexcept
{
	switch (profile)
	{
	case DeploymentProfile::Development:
		return "development";
	case DeploymentProfile::Engineering:
		return "engineering";
	case DeploymentProfile::Production:
		return "production";
	}
	return "invalid";
}

[[nodiscard]] constexpr bool factoryConfigurationLocked(const DeploymentProfile profile,
	const bool securityProvisioned,
	const bool manufacturingProvisioned) noexcept
{
	return profile == DeploymentProfile::Production && (securityProvisioned || manufacturingProvisioned);
}
}