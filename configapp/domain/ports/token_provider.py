from abc import ABC, abstractmethod
from typing import Dict, Any

class TokenProvider(ABC):
    @abstractmethod
    def generate_token(self, payload: Dict[str, Any]) -> str:
        pass

    @abstractmethod
    def validate_token(self, token: str) -> Dict[str, Any]:
        pass