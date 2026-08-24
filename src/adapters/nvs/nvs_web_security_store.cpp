#include "nvs_web_security_store.h"

#include <array>
#include <cstring>

namespace switch_actuator::adapters::nvs
{
namespace
{
constexpr char securityNamespace[]{"web_security"};
constexpr char slotAKey[]{"record_a"};
constexpr char slotBKey[]{"record_b"};
constexpr char activeKey[]{"active"};
constexpr std::uint8_t slotA{0};
constexpr std::uint8_t slotB{1};
constexpr std::uint32_t recordMagic{0x5745'4253U};

struct StoredRecord final
{
	std::uint32_t magic{recordMagic};
	std::uint32_t generation{0};
	ports::WebSecurityRecord record{};
	std::uint32_t checksum{0};
};

std::uint32_t checksum(const StoredRecord &stored) noexcept
{
	const auto *bytes = reinterpret_cast<const std::uint8_t *>(&stored);
	constexpr auto checkedSize = sizeof(StoredRecord) - sizeof(stored.checksum);
	std::uint32_t value{2166136261U};
	for (std::size_t index = 0; index < checkedSize; ++index)
	{
		value = (value ^ bytes[index]) * 16777619U;
	}
	return value;
}

bool readRecord(Preferences &preferences, const char *key, StoredRecord &stored) noexcept
{
	stored = {};
	return preferences.getBytesLength(key) == sizeof(stored) &&
		preferences.getBytes(key, &stored, sizeof(stored)) == sizeof(stored) && stored.magic == recordMagic &&
		stored.checksum == checksum(stored);
}
}

NvsWebSecurityStore::~NvsWebSecurityStore()
{
	if (initialized_) preferences_.end();
}

bool NvsWebSecurityStore::initialize() noexcept
{
	if (!initialized_) initialized_ = preferences_.begin(securityNamespace, false);
	return initialized_;
}

ports::WebSecurityStore NvsWebSecurityStore::port() noexcept
{
	return {loadHandler, saveHandler, eraseHandler, this};
}

bool NvsWebSecurityStore::isInitialized() const noexcept { return initialized_; }

ports::WebSecurityStoreResult NvsWebSecurityStore::loadHandler(void *const context,
	ports::WebSecurityRecord &record) noexcept
{
	return static_cast<NvsWebSecurityStore *>(context)->load(record);
}

ports::WebSecurityStoreResult NvsWebSecurityStore::saveHandler(void *const context,
	const ports::WebSecurityRecord &record) noexcept
{
	return static_cast<NvsWebSecurityStore *>(context)->save(record);
}

ports::WebSecurityStoreResult NvsWebSecurityStore::eraseHandler(void *const context) noexcept
{
	return static_cast<NvsWebSecurityStore *>(context)->erase();
}

ports::WebSecurityStoreResult NvsWebSecurityStore::load(ports::WebSecurityRecord &record) noexcept
{
	if (!initialized_) return ports::WebSecurityStoreResult::IoFailure;
	StoredRecord first{};
	StoredRecord second{};
	const auto firstValid = readRecord(preferences_, slotAKey, first);
	const auto secondValid = readRecord(preferences_, slotBKey, second);
	if (!firstValid && !secondValid)
	{
		return preferences_.getBytesLength(slotAKey) == 0 && preferences_.getBytesLength(slotBKey) == 0 ?
			ports::WebSecurityStoreResult::NotFound : ports::WebSecurityStoreResult::InvalidRecord;
	}
	record = firstValid && (!secondValid || first.generation >= second.generation) ? first.record : second.record;
	return ports::WebSecurityStoreResult::Applied;
}

ports::WebSecurityStoreResult NvsWebSecurityStore::save(const ports::WebSecurityRecord &record) noexcept
{
	if (!initialized_) return ports::WebSecurityStoreResult::IoFailure;
	StoredRecord first{};
	StoredRecord second{};
	const auto firstValid = readRecord(preferences_, slotAKey, first);
	const auto secondValid = readRecord(preferences_, slotBKey, second);
	const auto active = firstValid && (!secondValid || first.generation >= second.generation) ? slotA : slotB;
	StoredRecord stored{};
	stored.generation = std::max(firstValid ? first.generation : 0U, secondValid ? second.generation : 0U) + 1U;
	stored.record = record;
	stored.checksum = checksum(stored);
	const auto target = active == slotA ? slotB : slotA;
	const auto *key = target == slotA ? slotAKey : slotBKey;
	if (preferences_.putBytes(key, &stored, sizeof(stored)) != sizeof(stored))
		return ports::WebSecurityStoreResult::IoFailure;
	StoredRecord verified{};
	if (!readRecord(preferences_, key, verified) || std::memcmp(&stored, &verified, sizeof(stored)) != 0)
		return ports::WebSecurityStoreResult::InvalidRecord;
	return preferences_.putUChar(activeKey, target) == sizeof(target) ? ports::WebSecurityStoreResult::Applied :
		ports::WebSecurityStoreResult::IoFailure;
}

ports::WebSecurityStoreResult NvsWebSecurityStore::erase() noexcept
{
	return initialized_ && preferences_.clear() ? ports::WebSecurityStoreResult::Applied :
		ports::WebSecurityStoreResult::IoFailure;
}
}