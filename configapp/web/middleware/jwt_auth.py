from flask import request, jsonify, g
from domain.services.jwt_provider import JwtTokenProvider, TokenExpiredError, InvalidTokenError

def create_jwt_middleware(jwt_provider: JwtTokenProvider):
    def middleware():
        # 1. Исключаем публичные эндпоинты аутентификации
        if request.blueprint == 'auth':
            return None
            
        # 2. Исключаем эндпоинты обмена, защищенные HMAC-токенами
        if request.blueprint == 'export' and request.path in [
            '/api/v1/export/download', 
            '/api/v1/export/ack'
        ]:
            return None
            
        # 3. Проверка JWT для всех остальных защищенных эндпоинтов
        auth_header = request.headers.get('Authorization')
        if not auth_header or not auth_header.startswith('Bearer '):
            return jsonify({"error": "Missing or invalid Authorization header"}), 401
        
        token = auth_header.split(' ')[1]
        try:
            payload = jwt_provider.decode_token(token)
            if payload.get('type') != 'access':
                return jsonify({"error": "Invalid token type"}), 401
            
            g.current_user = payload.get('sub')
        except TokenExpiredError:
            return jsonify({"error": "Token has expired"}), 401
        except InvalidTokenError:
            return jsonify({"error": "Invalid token"}), 401
            
        return None

    return middleware