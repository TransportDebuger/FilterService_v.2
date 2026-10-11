import jwt
from datetime import datetime, timedelta, timezone
from typing import Any, Dict

class TokenExpiredError(Exception):
    pass

class InvalidTokenError(Exception):
    pass

class JwtTokenProvider:
    def __init__(
        self, 
        secret: str, 
        algorithm: str, 
        access_expires_minutes: int, 
        refresh_expires_days: int
    ):
        self._secret = secret
        self._algorithm = algorithm
        self._access_expires = timedelta(minutes=access_expires_minutes)
        self._refresh_expires = timedelta(days=refresh_expires_days)

    def generate_access_token(self, subject: str) -> str:
        return self._generate_token(subject, self._access_expires, token_type="access")

    def generate_refresh_token(self, subject: str) -> str:
        return self._generate_token(subject, self._refresh_expires, token_type="refresh")

    def decode_token(self, token: str) -> Dict[str, Any]:
        try:
            return jwt.decode(token, self._secret, algorithms=[self._algorithm])
        except jwt.ExpiredSignatureError as e:
            raise TokenExpiredError("Token has expired") from e
        except jwt.InvalidTokenError as e:
            raise InvalidTokenError(f"Invalid token: {e}") from e

    def _generate_token(self, subject: str, expires_delta: timedelta, token_type: str) -> str:
        now = datetime.now(timezone.utc)
        payload = {
            "sub": subject,
            "iat": now,
            "exp": now + expires_delta,
            "type": token_type
        }
        return jwt.encode(payload, self._secret, algorithm=self._algorithm)