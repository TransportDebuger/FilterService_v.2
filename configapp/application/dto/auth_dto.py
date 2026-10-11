from dataclasses import dataclass

@dataclass(frozen=True)
class AuthResponseDTO:
    access_token: str
    refresh_token: str