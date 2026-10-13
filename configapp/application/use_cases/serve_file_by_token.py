from pathlib import Path
from domain.ports.token_provider import TokenProvider

class ServeFileByTokenUseCase:
    def __init__(self, token_provider: TokenProvider):
        self._token_provider = token_provider

    def execute(self, token: str) -> str:
        payload = self._token_provider.validate_token(token)
        if payload.get("action") != "download":
            raise ValueError("Invalid token action")
            
        file_path = payload.get("file_path")
        if not file_path or not Path(file_path).exists():
            raise FileNotFoundError("File not found")
            
        return file_path