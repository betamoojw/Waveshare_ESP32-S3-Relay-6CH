#pragma once

namespace switch_actuator::ports
{
template <typename Event>
class EventSink final
{
public:
	using Handler = bool (*)(void *context, const Event &event) noexcept;

	constexpr EventSink() noexcept = default;

	constexpr EventSink(Handler handler, void *context = nullptr) noexcept
		: handler_{handler}, context_{context}
	{
	}

	[[nodiscard]] bool publish(const Event &event) const noexcept
	{
		return handler_ != nullptr && handler_(context_, event);
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return handler_ != nullptr;
	}

private:
	Handler handler_{nullptr};
	void *context_{nullptr};
};
}