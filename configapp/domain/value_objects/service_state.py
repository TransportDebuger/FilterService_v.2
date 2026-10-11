from dataclasses import dataclass
from typing import Optional

@dataclass(frozen=True)
class ServiceState:
    is_active: bool
    pid: Optional[int]
    status_text: str