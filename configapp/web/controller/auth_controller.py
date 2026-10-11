from flask import Blueprint, request, jsonify
from application.use_cases.authenticate_user import AuthenticateUserUseCase, AuthenticationError
from domain.services.jwt_provider import JwtTokenProvider, TokenExpiredError, InvalidTokenError

auth_bp = Blueprint('auth', __name__, url_prefix='/api/v1/auth')

class AuthController:
    def __init__(self, authenticate_use_case: AuthenticateUserUseCase, jwt_provider: JwtTokenProvider):
        self._authenticate_use_case = authenticate_use_case
        self._jwt_provider = jwt_provider

    def register_routes(self):
        @auth_bp.route('/login', methods=['POST'])
        def login():
            data = request.get_json()
            if not data or 'username' not in data or 'password' not in data:
                return jsonify({"error": "Username and password are required"}), 400
            
            try:
                dto = self._authenticate_use_case.execute(data['username'], data['password'])
                return jsonify({
                    "access_token": dto.access_token,
                    "refresh_token": dto.refresh_token
                }), 200
            except AuthenticationError as e:
                return jsonify({"error": str(e)}), 401

        @auth_bp.route('/refresh', methods=['POST'])
        def refresh():
            data = request.get_json()
            if not data or 'refresh_token' not in data:
                return jsonify({"error": "Refresh token is required"}), 400
            
            try:
                payload = self._jwt_provider.decode_token(data['refresh_token'])
                if payload.get('type') != 'refresh':
                    return jsonify({"error": "Invalid token type"}), 401
                
                username = payload.get('sub')
                new_access = self._jwt_provider.generate_access_token(subject=username)
                new_refresh = self._jwt_provider.generate_refresh_token(subject=username)
                
                return jsonify({
                    "access_token": new_access,
                    "refresh_token": new_refresh
                }), 200
            except TokenExpiredError:
                return jsonify({"error": "Refresh token expired"}), 401
            except InvalidTokenError:
                return jsonify({"error": "Invalid refresh token"}), 401