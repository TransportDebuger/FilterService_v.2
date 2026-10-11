from dataclasses import dataclass
from typing import Any, Dict

@dataclass(frozen=True)
class ConfigResponseDTO:
    config: Dict[str, Any]
    etag: str