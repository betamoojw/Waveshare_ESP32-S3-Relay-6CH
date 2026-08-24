#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::ports
{
inline constexpr std::size_t webUserCapacity{4};
inline constexpr std::size_t webSessionCapacity{2};
inline constexpr std::size_t webUsernameCapacity{65};
inline constexpr std::size_t webPasswordSaltSize{16};
inline constexpr std::size_t webPasswordVerifierSize{32};
inline constexpr std::size_t webSigningKeySize{32};
inline constexpr std::size_t webCertificateCapacity{3073};
inline constexpr std::size_t webPrivateKeyCapacity{2049};

enum class WebUserRole : std::uint8_t
{
	Guest,
	Administrator
};

struct WebUserRecord final
{
	std::uint32_t id{0};
	std::array<char, webUsernameCapacity> username{};
	WebUserRole role{WebUserRole::Guest};
	bool enabled{false};
	std::array<std::uint8_t, webPasswordSaltSize> passwordSalt{};
	std::array<std::uint8_t, webPasswordVerifierSize> passwordVerifier{};
	std::uint32_t passwordIterations{0};
};

struct WebSecurityRecord final
{
	std::uint32_t schemaVersion{1};
	std::uint32_t signingGeneration{1};
	std::array<std::uint8_t, webSigningKeySize> signingKey{};
	std::array<WebUserRecord, webUserCapacity> users{};
	std::array<char, webCertificateCapacity> certificate{};
	std::array<char, webPrivateKeyCapacity> privateKey{};
};

enum class WebSecurityStoreResult : std::uint8_t
{
	Applied,
	NotFound,
	InvalidRecord,
	IoFailure
};

using WebSecurityLoadHandler = WebSecurityStoreResult (*)(void *context, WebSecurityRecord &record) noexcept;
using WebSecuritySaveHandler = WebSecurityStoreResult (*)(void *context, const WebSecurityRecord &record) noexcept;
using WebSecurityEraseHandler = WebSecurityStoreResult (*)(void *context) noexcept;

class WebSecurityStore final
{
public:
	constexpr WebSecurityStore() noexcept = default;
	constexpr WebSecurityStore(WebSecurityLoadHandler loadHandler,
		WebSecuritySaveHandler saveHandler,
		WebSecurityEraseHandler eraseHandler,
		void *context = nullptr) noexcept
		: loadHandler_{loadHandler}, saveHandler_{saveHandler}, eraseHandler_{eraseHandler}, context_{context}
	{
	}

	[[nodiscard]] WebSecurityStoreResult load(WebSecurityRecord &record) const noexcept
	{
		return loadHandler_ != nullptr ? loadHandler_(context_, record) : WebSecurityStoreResult::IoFailure;
	}
	[[nodiscard]] WebSecurityStoreResult save(const WebSecurityRecord &record) const noexcept
	{
		return saveHandler_ != nullptr ? saveHandler_(context_, record) : WebSecurityStoreResult::IoFailure;
	}
	[[nodiscard]] WebSecurityStoreResult erase() const noexcept
	{
		return eraseHandler_ != nullptr ? eraseHandler_(context_) : WebSecurityStoreResult::IoFailure;
	}
	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return loadHandler_ != nullptr && saveHandler_ != nullptr && eraseHandler_ != nullptr;
	}

private:
	WebSecurityLoadHandler loadHandler_{nullptr};
	WebSecuritySaveHandler saveHandler_{nullptr};
	WebSecurityEraseHandler eraseHandler_{nullptr};
	void *context_{nullptr};
};
}