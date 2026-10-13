import hmac
import hashlib
import json
import base64
import time
from domain.ports.token_provider import TokenProvider

class HmacTokenProvider(TokenProvider):
    """
    Генерация и валидация токенов на основе HMAC-SHA256.
    Используется для подписывания ссылок на скачивание и ACK-запросов.
    """
    def __init__(self, secret: str, ttl_seconds: int = 3600):
        self._secret = secret.encode('utf-8')
        self._ttl = ttl_seconds

    def generate_token(self, payload: dict) -> str:
        payload['exp'] = int(time.time()) + self._ttl
        data = json.dumps(payload, sort_keys=True).encode('utf-8')
        signature = hmac.new(self._secret, data, hashlib.sha256).hexdigest()
        
        token_data = {
            'payload': base64.urlsafe_b64encode(data).decode('utf-8'),
            'signature': signature
        }
        return base64.urlsafe_b64encode(json.dumps(token_data).encode('utf-8')).decode('utf-8')

    def validate_token(self, token: str) -> dict:
        try:
            token_data = json.loads(base64.urlsafe_b64decode(token.encode('utf-8')))
            payload_b64 = token_data['payload']
            signature = token_data['signature']
            
            data = base64.urlsafe_b64decode(payload_b64.encode('utf-8'))
            expected_signature = hmac.new(self._secret, data, hashlib.sha256).hexdigest()
            
            if not hmac.compare_digest(signature, expected_signature):
                raise ValueError("Invalid signature")
                
            payload = json.loads(data)
            if payload.get('exp', 0) < time.time():
                raise ValueError("Token expired")
                
            return payload
        except Exception as e:
            raise ValueError(f"Invalid token: {e}")