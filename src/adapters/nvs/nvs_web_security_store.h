#pragma once

#include "../../ports/web_security_store.h"

#include <Preferences.h>

#include <cstdint>

namespace switch_actuator::adapters::nvs
{
class NvsWebSecurityStore final
{
public:
	NvsWebSecurityStore() noexcept = default;
	~NvsWebSecurityStore();

	NvsWebSecurityStore(const NvsWebSecurityStore &) = delete;
	NvsWebSecurityStore &operator=(const NvsWebSecurityStore &) = delete;

	[[nodiscard]] bool initialize() noexcept;
	[[nodiscard]] ports::WebSecurityStore port() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static ports::WebSecurityStoreResult loadHandler(void *context,
		ports::WebSecurityRecord &record) noexcept;
	[[nodiscard]] static ports::WebSecurityStoreResult saveHandler(void *context,
		const ports::WebSecurityRecord &record) noexcept;
	[[nodiscard]] static ports::WebSecurityStoreResult eraseHandler(void *context) noexcept;
	[[nodiscard]] ports::WebSecurityStoreResult load(ports::WebSecurityRecord &record) noexcept;
	[[nodiscard]] ports::WebSecurityStoreResult save(const ports::WebSecurityRecord &record) noexcept;
	[[nodiscard]] ports::WebSecurityStoreResult erase() noexcept;

	Preferences preferences_{};
	bool initialized_{false};
};
}