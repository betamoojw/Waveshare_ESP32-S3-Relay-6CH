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
SECRET_KEY_PARTS = ("password", "passwd", "privatekey", "private_key", "psk", "secret", "token")


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


def sign_production_image(_source, _target, environment):
	unsigned_image = Path(environment.subst("$BUILD_DIR/${PROGNAME}.bin")).resolve()
	signed_image = unsigned_image.with_name("%s-signed.bin" % unsigned_image.stem)
	esptool_package = Path(env.PioPlatform().get_package_dir("tool-esptoolpy"))
	espsecure = esptool_package / "espsecure.py"
	if not espsecure.is_file():
		fail("ESP32 espsecure signer is unavailable")
	command = [sys.executable, str(espsecure), "sign-data", "--version", "2",
		"--keyfile", str(production_signing_key), "--output", str(signed_image), str(unsigned_image)]
	subprocess.run(command, check=True)
	print("Signed production firmware: %s" % signed_image)


env.Replace(PROGNAME="firmware_%s_%s" % (build_tag, artifact_version_tag))
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version_tag))])

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
	configuration_path = Path(env.subst("$PROJECT_DIR")) / "config" / "default_configuration.json"
	try:
		configuration = json.loads(configuration_path.read_text(encoding="utf-8"))
	except (OSError, ValueError) as error:
		fail("embedded configuration cannot be validated: %s" % error)
	secret_path = find_embedded_secret(configuration)
	if secret_path:
		fail("embedded configuration contains a credential at %s" % secret_path)
	env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign_production_image)