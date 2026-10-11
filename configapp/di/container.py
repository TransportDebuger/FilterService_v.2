import json
from pathlib import Path
from datasource.env.env_reader import EnvReader
from datasource.file_system.file_config_repository import FileConfigRepository
from datasource.file_system.file_user_repository import FileUserRepository
from infrastructure.os.systemd_service_controller import SystemdServiceController
from infrastructure.network.prometheus_health_check_client import PrometheusHealthCheckClient
from domain.services.config_validator import ConfigValidator
from domain.services.jwt_provider import JwtTokenProvider

from application.use_cases.authenticate_user import AuthenticateUserUseCase
from application.use_cases.get_configuration import GetConfigurationUseCase
from application.use_cases.update_configuration import UpdateConfigurationUseCase
from application.use_cases.get_service_status import GetServiceStatusUseCase
from application.use_cases.restart_service import RestartServiceUseCase

class Container:
    """Контейнер внедрения зависимостей. Инициализирует граф объектов на основе .env."""

    def __init__(self):
        self._env_reader = EnvReader(Path('.env'))
        self._env_reader.load()

        # Инициализация адаптеров (Datasource & Infrastructure)
        self._config_repo = FileConfigRepository(
            config_path=Path(self._env_reader.get('CONFIG_PATH')),
            backup_dir=Path(self._env_reader.get('BACKUP_DIR'))
        )
        self._user_repo = FileUserRepository(
            users_file=Path(self._env_reader.get('USERS_FILE'))
        )
        self._service_controller = SystemdServiceController(
            pid_file=Path(self._env_reader.get('SERVICE_PID_FILE')),
            service_name=self._env_reader.get('SERVICE_NAME', 'xmlfilter')
        )
        self._health_check = PrometheusHealthCheckClient(
            metrics_url=f"http://localhost:{self._env_reader.get('SERVICE_METRICS_PORT')}/metrics"
        )

        # Инициализация доменных сервисов
        schema_path = self._env_reader.get('CONFIG_SCHEMA_PATH')
        with open(schema_path, 'r', encoding='utf-8') as f:
            schema = json.load(f)
            
        self._config_validator = ConfigValidator(schema=schema)
        self._jwt_provider = JwtTokenProvider(
            secret=self._env_reader.get('JWT_SECRET'),
            algorithm=self._env_reader.get('JWT_ALGORITHM'),
            access_expires_minutes=int(self._env_reader.get('JWT_ACCESS_TOKEN_EXPIRES_MINUTES')),
            refresh_expires_days=int(self._env_reader.get('JWT_REFRESH_TOKEN_EXPIRES_DAYS'))
        )

    # Фабрики Use Cases
    def authenticate_user_use_case(self) -> AuthenticateUserUseCase:
        return AuthenticateUserUseCase(self._user_repo, self._jwt_provider)

    def get_configuration_use_case(self) -> GetConfigurationUseCase:
        return GetConfigurationUseCase(self._config_repo)

    def update_configuration_use_case(self) -> UpdateConfigurationUseCase:
        return UpdateConfigurationUseCase(
            self._config_repo, 
            self._config_validator, 
            self._service_controller
        )

    def get_service_status_use_case(self) -> GetServiceStatusUseCase:
        return GetServiceStatusUseCase(self._service_controller)

    def restart_service_use_case(self) -> RestartServiceUseCase:
        return RestartServiceUseCase(self._service_controller)