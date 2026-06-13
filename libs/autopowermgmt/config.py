import json
from dataclasses import dataclass
from pathlib import Path

# resolve paths relative to THIS file, not CWD
_BASE_DIR = Path(__file__).resolve().parent


DEFAULT_CONFIG_PATH = _BASE_DIR / "config/config.json"
DEFAULT_SECRETS_PATH = _BASE_DIR / "config/secrets.json"


@dataclass(frozen=True)
class Config:
    host: str
    port: int
    mgmt_id: str | None = None


@dataclass(frozen=True)
class Secrets:
    key: str
    cert: str
    ca: str | None = None
    mgmt_secret: str | None = None


def load_config(path: str | Path | None = None) -> Config:
    path = Path(path) if path else DEFAULT_CONFIG_PATH

    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)

    host = data.get("remoteHost")
    port = data.get("remotePort")
    mgmt_id = data.get("mgmtId")

    if not host or not port:
        raise ValueError("config.json must contain remoteHost and remotePort")

    return Config(host=host, port=int(port), mgmt_id=mgmt_id)


def load_secrets(path: str | Path | None = None) -> Secrets:
    path = Path(path) if path else DEFAULT_SECRETS_PATH

    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)

    ssl = data.get("ssl", {})

    key = ssl.get("privKeyPath")
    cert = ssl.get("pubKeyPath")
    ca = ssl.get("pubKeyCA")

    mgmt_secret = data.get("mgmtSecret")

    if not key or not cert:
        raise ValueError("secrets.json must contain ssl.privKeyPath and ssl.pubKeyPath")

    return Secrets(
        key=key,
        cert=cert,
        ca=ca,
        mgmt_secret=mgmt_secret,
    )
