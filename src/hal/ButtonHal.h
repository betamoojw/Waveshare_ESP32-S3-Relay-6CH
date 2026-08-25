#pragma once

namespace switch_actuator::hal
{
using ButtonInitializeHandler = bool (*)(void *context) noexcept;
using ButtonPressedHandler = bool (*)(void *context) noexcept;

class ButtonHal final
{
public:
	constexpr ButtonHal() noexcept = default;
	constexpr ButtonHal(const ButtonInitializeHandler initialize,
		const ButtonPressedHandler pressed,
		void *const context = nullptr) noexcept
		: initialize_{initialize}, pressed_{pressed}, context_{context}
	{
	}

	[[nodiscard]] bool initialize() const noexcept { return initialize_ != nullptr && initialize_(context_); }
	[[nodiscard]] bool isPressed() const noexcept { return pressed_ != nullptr && pressed_(context_); }
	[[nodiscard]] constexpr bool isValid() const noexcept { return initialize_ != nullptr && pressed_ != nullptr; }

private:
	ButtonInitializeHandler initialize_{nullptr};
	ButtonPressedHandler pressed_{nullptr};
	void *context_{nullptr};
};
}