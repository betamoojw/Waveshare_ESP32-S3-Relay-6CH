"""Generate a protected initial-administrator credential file."""

from __future__ import annotations

import argparse
import secrets
import string
from pathlib import Path
from typing import Dict, Optional, Sequence

try:
    from .factory_common import atomic_write_json
except ImportError:
    from factory_common import atomic_write_json


_ALPHABET = string.ascii_letters + string.digits + "-._~!@#$%^&*"


def generate_credentials(username: str = "admin", length: int = 24) -> Dict[str, str]:
    if not username or len(username) > 31 or any(character.isspace() for character in username):
        raise ValueError("administrator username must contain 1-31 non-whitespace characters")
    if not 16 <= length <= 128:
        raise ValueError("credential length must be in the range 16..128")
    while True:
        password = "".join(secrets.choice(_ALPHABET) for _ in range(length))
        if (any(character.islower() for character in password)
                and any(character.isupper() for character in password)
                and any(character.isdigit() for character in password)
                and any(character in "-._~!@#$%^&*" for character in password)):
            return {"username": username, "password": password}


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--username", default="admin")
    parser.add_argument("--length", type=int, default=24)
    parser.add_argument("--output", required=True, type=Path)
    options = parser.parse_args(arguments)
    credentials = generate_credentials(options.username, options.length)
    atomic_write_json(options.output, credentials, private=True, allow_secrets=True)
    print(f"generated protected credentials for {credentials['username']}: {options.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())