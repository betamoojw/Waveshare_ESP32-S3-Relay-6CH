#include "null_knx_adapter.h"

namespace switch_actuator::adapters::knx
{
NullKnxAdapter::NullKnxAdapter(app::DiagnosticsService *const diagnostics) noexcept
	: diagnostics_{diagnostics}
{
}

KnxInitializeResult NullKnxAdapter::initialize(const bool enabledByConfiguration, const std::uint32_t nowMs) noexcept
{
	requested_ = enabledByConfiguration;
	initialized_ = false;
	if (diagnostics_ == nullptr)
	{
		return KnxInitializeResult::InvalidDependencies;
	}

	diagnostics_->updateKnx(false, false);
	if (!requested_)
	{
		static_cast<void>(diagnostics_->clearFault(domain::FaultCode::KnxUnavailable));
		initialized_ = true;
		return KnxInitializeResult::Disabled;
	}

	static_cast<void>(diagnostics_->recordFault(domain::FaultCode::KnxUnavailable, domain::FaultSeverity::Warning, nowMs));
	initialized_ = true;
	return KnxInitializeResult::Unavailable;
}

KnxPollResult NullKnxAdapter::poll() const noexcept
{
	if (!initialized_)
	{
		return KnxPollResult::NotInitialized;
	}
	return requested_ ? KnxPollResult::Unavailable : KnxPollResult::Idle;
}

bool NullKnxAdapter::isInitialized() const noexcept
{
	return initialized_;
}
}