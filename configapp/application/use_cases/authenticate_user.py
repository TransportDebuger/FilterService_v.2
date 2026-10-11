import bcrypt
from domain.ports.user_repository import UserRepository
from domain.services.jwt_provider import JwtTokenProvider
from application.dto.auth_dto import AuthResponseDTO

class AuthenticationError(Exception):
    pass

class AuthenticateUserUseCase:
    def __init__(self, user_repo: UserRepository, jwt_provider: JwtTokenProvider):
        self._user_repo = user_repo
        self._jwt_provider = jwt_provider

    def execute(self, username: str, password: str) -> AuthResponseDTO:
        user = self._user_repo.get_user(username)
        if user is None:
            raise AuthenticationError("Invalid username or password")

        # Проверка хеша пароля
        if not bcrypt.checkpw(password.encode('utf-8'), user.password_hash.encode('utf-8')):
            raise AuthenticationError("Invalid username or password")

        access_token = self._jwt_provider.generate_access_token(subject=username)
        refresh_token = self._jwt_provider.generate_refresh_token(subject=username)

        return AuthResponseDTO(access_token=access_token, refresh_token=refresh_token)