"""Versioned, failure-atomic production provisioning records."""

from __future__ import annotations

import hashlib
import json
import os
import stat
import tempfile
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path
from typing import Any, Dict, Optional


class DeploymentProfile(str, Enum):
    DEVELOPMENT = "development"
    ENGINEERING = "engineering"
    PRODUCTION = "production"


class ProvisioningState(str, Enum):
    FIRMWARE_FLASHED = "firmware-flashed"
    IDENTITY_GENERATED = "identity-generated"
    FACTORY_CONFIGURATION_WRITTEN = "factory-configuration-written"
    SECURITY_PROVISIONED = "security-provisioned"
    VERIFIED = "verified"
    LOCKED = "locked"


_STATE_ORDER = tuple(ProvisioningState)


@dataclass(frozen=True)
class ProductionIdentity:
    serial_number: str
    device_uuid: str
    product_id: str
    hardware_revision: str
    manufacturing_date: str
    manufacturing_batch: int


@dataclass
class ProductionManifest:
    identity: ProductionIdentity
    profile: DeploymentProfile = DeploymentProfile.PRODUCTION
    state: ProvisioningState = ProvisioningState.FIRMWARE_FLASHED
    firmware_sha256: str = ""
    created_at: str = ""
    updated_at: str = ""
    schema_version: int = 1

    def __post_init__(self) -> None:
        now = _utc_now()
        if not self.created_at:
            self.created_at = now
        if not self.updated_at:
            self.updated_at = now
        if self.profile is not DeploymentProfile.PRODUCTION:
            raise ValueError("production manifests require the production deployment profile")

    def advance(self, state: ProvisioningState) -> None:
        current_index = _STATE_ORDER.index(self.state)
        next_index = _STATE_ORDER.index(state)
        if self.state is ProvisioningState.LOCKED:
            raise ValueError("locked production manifests cannot be changed")
        if next_index != current_index + 1:
            raise ValueError(f"invalid provisioning transition: {self.state.value} -> {state.value}")
        self.state = state
        self.updated_at = _utc_now()

    def to_dict(self) -> Dict[str, Any]:
        result = asdict(self)
        result["profile"] = self.profile.value
        result["state"] = self.state.value
        return result

    @classmethod
    def from_dict(cls, value: Dict[str, Any]) -> "ProductionManifest":
        if value.get("schema_version") != 1:
            raise ValueError("unsupported production manifest schema")
        return cls(
            identity=ProductionIdentity(**value["identity"]),
            profile=DeploymentProfile(value["profile"]),
            state=ProvisioningState(value["state"]),
            firmware_sha256=value.get("firmware_sha256", ""),
            created_at=value["created_at"],
            updated_at=value["updated_at"],
            schema_version=value["schema_version"],
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_manifest(path: Path) -> ProductionManifest:
    with path.open("r", encoding="utf-8") as source:
        return ProductionManifest.from_dict(json.load(source))


def save_manifest(path: Path, manifest: ProductionManifest) -> None:
    if path.exists() and load_manifest(path).state is ProvisioningState.LOCKED:
        raise ValueError("locked production manifests cannot be overwritten")
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=str(path.parent), text=True)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(manifest.to_dict(), output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_name, path)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)


def lock_manifest(path: Path, manifest: ProductionManifest) -> None:
    manifest.advance(ProvisioningState.LOCKED)
    save_manifest(path, manifest)
    path.chmod(stat.S_IREAD | stat.S_IRGRP | stat.S_IROTH)


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()
