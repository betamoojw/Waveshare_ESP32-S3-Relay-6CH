#include "esp32_web_crypto.h"

#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace switch_actuator::adapters::web
{
namespace
{
int hardwareRandom(void *, unsigned char *const output, const std::size_t size) noexcept
{
	if (output == nullptr) return -1;
	esp_fill_random(output, size);
	return 0;
}
}

ports::WebCryptoPort Esp32WebCrypto::port() noexcept
{
	return {random, hmacSha256, verifyPassword, derivePassword, generateIdentity, this};
}

bool Esp32WebCrypto::random(void *, std::uint8_t *const output, const std::size_t size) noexcept
{
	if (output == nullptr || size == 0) return false;
	esp_fill_random(output, size);
	return true;
}

bool Esp32WebCrypto::hmacSha256(void *,
	const std::uint8_t *const key,
	const std::size_t keySize,
	const std::string_view message,
	std::uint8_t *const output,
	const std::size_t outputSize) noexcept
{
	const auto *algorithm = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	return algorithm != nullptr && key != nullptr && output != nullptr && outputSize == 32U &&
		mbedtls_md_hmac(algorithm, key, keySize,
			reinterpret_cast<const std::uint8_t *>(message.data()), message.size(), output) == 0;
}

bool Esp32WebCrypto::verifyPassword(void *,
	const std::string_view password,
	const std::uint8_t *const salt,
	const std::size_t saltSize,
	const std::uint32_t iterations,
	const std::uint8_t *const expected,
	const std::size_t expectedSize) noexcept
{
	if (salt == nullptr || expected == nullptr || expectedSize != 32U || iterations < 100'000U) return false;
	std::array<std::uint8_t, 32> derived{};
	if (mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
		reinterpret_cast<const std::uint8_t *>(password.data()), password.size(), salt, saltSize, iterations,
		derived.size(), derived.data()) != 0) return false;
	std::uint8_t difference{0};
	for (std::size_t index = 0; index < derived.size(); ++index) difference |= derived[index] ^ expected[index];
	return difference == 0;
}

bool Esp32WebCrypto::derivePassword(void *,
	const std::string_view password,
	const std::uint8_t *const salt,
	const std::size_t saltSize,
	const std::uint32_t iterations,
	std::uint8_t *const output,
	const std::size_t outputSize) noexcept
{
	return salt != nullptr && output != nullptr && outputSize == 32U && iterations >= 100'000U &&
		mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
			reinterpret_cast<const std::uint8_t *>(password.data()), password.size(), salt, saltSize, iterations,
			outputSize, output) == 0;
}

bool Esp32WebCrypto::generateIdentity(void *,
	const std::string_view hostName,
	char *const certificate,
	const std::size_t certificateCapacity,
	char *const privateKey,
	const std::size_t privateKeyCapacity) noexcept
{
	if (hostName.empty() || hostName.size() > 95U || certificate == nullptr || privateKey == nullptr) return false;
	std::fill_n(certificate, certificateCapacity, '\0');
	std::fill_n(privateKey, privateKeyCapacity, '\0');
	mbedtls_pk_context key{};
	mbedtls_x509write_cert writer{};
	mbedtls_pk_init(&key);
	mbedtls_x509write_crt_init(&writer);
	const auto cleanup = [&]() {
		mbedtls_x509write_crt_free(&writer);
		mbedtls_pk_free(&key);
	};
	const auto *keyInfo = mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY);
	if (keyInfo == nullptr || mbedtls_pk_setup(&key, keyInfo) != 0 ||
		mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), hardwareRandom, nullptr) != 0)
	{
		cleanup();
		return false;
	}
	std::array<std::uint8_t, 16> serialBytes{};
	esp_fill_random(serialBytes.data(), serialBytes.size());
	serialBytes[0] &= 0x7FU;
	serialBytes[0] |= 0x01U;
	char subject[160]{};
	if (std::snprintf(subject, sizeof(subject), "CN=%.*s,O=Switch Actuator", static_cast<int>(hostName.size()),
		hostName.data()) <= 0)
	{
		cleanup();
		return false;
	}
	mbedtls_x509_san_list san{};
	san.node.type = MBEDTLS_X509_SAN_DNS_NAME;
	san.node.san.unstructured_name.p = reinterpret_cast<unsigned char *>(const_cast<char *>(hostName.data()));
	san.node.san.unstructured_name.len = hostName.size();
	mbedtls_x509write_crt_set_subject_key(&writer, &key);
	mbedtls_x509write_crt_set_issuer_key(&writer, &key);
	mbedtls_x509write_crt_set_md_alg(&writer, MBEDTLS_MD_SHA256);
	mbedtls_x509write_crt_set_version(&writer, MBEDTLS_X509_CRT_VERSION_3);
	const auto valid = mbedtls_x509write_crt_set_subject_name(&writer, subject) == 0 &&
		mbedtls_x509write_crt_set_issuer_name(&writer, subject) == 0 &&
		mbedtls_x509write_crt_set_serial_raw(&writer, serialBytes.data(), serialBytes.size()) == 0 &&
		mbedtls_x509write_crt_set_validity(&writer, "20240101000000", "20491231235959") == 0 &&
		mbedtls_x509write_crt_set_basic_constraints(&writer, 0, -1) == 0 &&
		mbedtls_x509write_crt_set_key_usage(&writer, MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_AGREEMENT) == 0 &&
		mbedtls_x509write_crt_set_subject_alternative_name(&writer, &san) == 0 &&
		mbedtls_pk_write_key_pem(&key, reinterpret_cast<unsigned char *>(privateKey), privateKeyCapacity) == 0 &&
		mbedtls_x509write_crt_pem(&writer, reinterpret_cast<unsigned char *>(certificate), certificateCapacity,
			hardwareRandom, nullptr) == 0;
	cleanup();
	if (!valid)
	{
		std::fill_n(certificate, certificateCapacity, '\0');
		std::fill_n(privateKey, privateKeyCapacity, '\0');
	}
	return valid;
}
}