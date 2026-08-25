import json
import os
import re
import subprocess
import sys
from pathlib import Path

Import("env")


PRODUCTION_ENVIRONMENT = "production"
DEFAULT_FIRMWARE_VERSION = "FW-1.4.0"
SEMANTIC_FIRMWARE_VERSION = re.compile(
	r"(?:FW-)?v?(\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?)$")
REQUIRED_PRODUCTION_APPROVALS = (
	"SECURE_BOOT_PROVISIONING_APPROVED",
	"FLASH_ENCRYPTION_PROVISIONING_APPROVED",
)
KEY_DIGEST_PATTERN = re.compile(r"[0-9a-fA-F]{64}")
SECRET_KEY_PARTS = ("password", "passwd", "privatekey", "private_key", "psk", "secret", "token")
PRIVATE_KEY_MARKERS = (b"-----BEGIN PRIVATE KEY-----", b"-----BEGIN RSA PRIVATE KEY-----",
	b"-----BEGIN EC PRIVATE KEY-----")
PRIVATE_KEY_BLOCK = re.compile(
	br"-----BEGIN (?:ENCRYPTED |RSA |EC )?PRIVATE KEY-----[\s\S]+?-----END (?:ENCRYPTED |RSA |EC )?PRIVATE KEY-----")

# NOTE: strict compiler flags (-Werror, -Wstack-usage=8192) are enforced
# through static analysis gates (clang-tidy, cppcheck) which target only
# owned source. build_src_flags in platformio.ini applies -Wall and -Wextra
# to all source (including third-party libraries like nanomodbus) as warnings.


def exclude_psychic_middlewares(_environment, _node):
	return None


env.AddBuildMiddleware(exclude_psychic_middlewares, "*/PsychicMiddlewares.cpp")

build_tag = env['PIOENV']
requested_version = os.getenv("FIRMWARE_VERSION", "development").strip()


def normalized_firmware_version(value):
	match = SEMANTIC_FIRMWARE_VERSION.fullmatch(value)
	if match:
		return "FW-%s" % match.group(1)
	if value in ("", "development"):
		return "%s+development" % DEFAULT_FIRMWARE_VERSION
	if value == "main":
		return "%s+main" % DEFAULT_FIRMWARE_VERSION
	suffix = re.sub(r"[^0-9A-Za-z.-]+", ".", value).strip(".")
	return "%s+%s" % (DEFAULT_FIRMWARE_VERSION, suffix or "development")


version_tag = normalized_firmware_version(requested_version)
artifact_version_tag = requested_version or "development"


def fail(message):
	raise RuntimeError("Production build rejected: %s" % message)


def find_embedded_secret(value, path="root"):
	if isinstance(value, dict):
		for key, child in value.items():
			child_path = "%s.%s" % (path, key)
			normalized_key = key.lower().replace("-", "").replace("_", "")
			if any(part.replace("_", "") in normalized_key for part in SECRET_KEY_PARTS) and child not in (None, "", False):
				return child_path
			found = find_embedded_secret(child, child_path)
			if found:
				return found
	elif isinstance(value, list):
		for index, child in enumerate(value):
			found = find_embedded_secret(child, "%s[%d]" % (path, index))
			if found:
				return found
	return None


def validate_deployment_inputs(project_directory):
	configuration_paths = [project_directory / "config" / "default_configuration.json"]
	configuration_paths.extend(sorted((project_directory / "data" / "config").glob("*.json")))
	for configuration_path in configuration_paths:
		try:
			content = configuration_path.read_bytes()
			configuration = json.loads(content.decode("utf-8"))
		except (OSError, UnicodeError, ValueError) as error:
			fail("deployment configuration cannot be validated at %s: %s" % (configuration_path, error))
		secret_path = find_embedded_secret(configuration)
		if secret_path:
			fail("deployment configuration %s contains a credential at %s" % (configuration_path, secret_path))
		if any(marker in content for marker in PRIVATE_KEY_MARKERS):
			fail("deployment configuration contains private key material at %s" % configuration_path)
	for root_name in ("src", "include", "config", "data", "tools", ".github"):
		root = project_directory / root_name
		if not root.exists():
			continue
		for candidate in root.rglob("*"):
			if candidate.is_file() and candidate.stat().st_size <= 4 * 1024 * 1024:
				try:
					content = candidate.read_bytes()
				except OSError as error:
					fail("source secret scan cannot read %s: %s" % (candidate, error))
				if PRIVATE_KEY_BLOCK.search(content):
					fail("source tree contains a complete PEM private key at %s" % candidate)


def sign_production_image(_source, _target, environment):
	unsigned_image = Path(environment.subst("$BUILD_DIR/${PROGNAME}.bin")).resolve()
	signed_image = unsigned_image.with_name("%s-signed.bin" % unsigned_image.stem)
	if signed_image.exists():
		signed_image.unlink()
	command = [sys.executable, str(espsecure), "sign-data", "--version", "2",
		"--keyfile", str(production_signing_key), "--output", str(signed_image), str(unsigned_image)]
	subprocess.run(command, check=True)
	if not signed_image.is_file() or signed_image.stat().st_size <= unsigned_image.stat().st_size:
		fail("Secure Boot v2 signing did not produce a signed image")
	subprocess.run([sys.executable, str(espsecure), "verify-signature", "--version", "2",
		"--keyfile", str(production_signing_key), str(signed_image)], check=True)
	print("Signed production firmware: %s" % signed_image)


env.Replace(PROGNAME="firmware_%s_%s" % (build_tag, artifact_version_tag))
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version_tag))])
linker_map = Path(env.subst("$BUILD_DIR")) / (env.subst("${PROGNAME}") + ".map")
env.Append(LINKFLAGS=["-Wl,-Map,%s" % linker_map])

if build_tag == PRODUCTION_ENVIRONMENT:
	if SEMANTIC_FIRMWARE_VERSION.fullmatch(requested_version) is None:
		fail("FIRMWARE_VERSION must be an immutable semantic version such as FW-1.4.0")
	for approval in REQUIRED_PRODUCTION_APPROVALS:
		if os.getenv(approval) != "1":
			fail("%s=1 is required by the audited manufacturing workflow" % approval)
	key_value = os.getenv("PRODUCTION_SIGNING_KEY", "").strip()
	if not key_value:
		fail("PRODUCTION_SIGNING_KEY must reference an external Secure Boot v2 PEM key")
	production_signing_key = Path(key_value).expanduser().resolve()
	if not production_signing_key.is_file():
		fail("PRODUCTION_SIGNING_KEY does not reference a readable file")
	esptool_package = Path(env.PioPlatform().get_package_dir("tool-esptoolpy"))
	espsecure = esptool_package / "espsecure.py"
	if not espsecure.is_file():
		fail("ESP32 espsecure signer is unavailable")
	expected_key_digest = os.getenv("PRODUCTION_SIGNING_KEY_DIGEST", "").strip().lower()
	if KEY_DIGEST_PATTERN.fullmatch(expected_key_digest) is None:
		fail("PRODUCTION_SIGNING_KEY_DIGEST must be the approved 64-hex Secure Boot v2 public-key digest")
	digest_path = Path(env.subst("$BUILD_DIR")) / "secure-boot-key-digest.bin"
	digest_path.parent.mkdir(parents=True, exist_ok=True)
	subprocess.run([sys.executable, str(espsecure), "digest-sbv2-public-key",
		"--keyfile", str(production_signing_key), "--output", str(digest_path)], check=True)
	actual_key_digest = digest_path.read_bytes().hex()
	digest_path.unlink()
	if actual_key_digest != expected_key_digest:
		fail("PRODUCTION_SIGNING_KEY does not match the approved production key digest")
	validate_deployment_inputs(Path(env.subst("$PROJECT_DIR")))
	env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign_production_image)