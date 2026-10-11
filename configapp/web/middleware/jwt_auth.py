from flask import request, jsonify, g
from domain.services.jwt_provider import JwtTokenProvider, TokenExpiredError, InvalidTokenError

def create_jwt_middleware(jwt_provider: JwtTokenProvider):
    """
    Фабрика middleware для проверки JWT-токенов.
    Интегрируется в Flask через @app.before_request.
    """
    def middleware():
        # Исключаем публичные эндпоинты (Blueprint 'auth')
        if request.blueprint == 'auth':
            return None
            
        auth_header = request.headers.get('Authorization')
        if not auth_header or not auth_header.startswith('Bearer '):
            return jsonify({"error": "Missing or invalid Authorization header"}), 401
        
        token = auth_header.split(' ')[1]
        try:
            payload = jwt_provider.decode_token(token)
            if payload.get('type') != 'access':
                return jsonify({"error": "Invalid token type"}), 401
            
            # Сохранение идентификатора пользователя в контексте запроса
            g.current_user = payload.get('sub')
        except TokenExpiredError:
            return jsonify({"error": "Token has expired"}), 401
        except InvalidTokenError:
            return jsonify({"error": "Invalid token"}), 401
            
        return None

    return middleware