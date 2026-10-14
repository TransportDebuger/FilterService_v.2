import json
import logging
from pathlib import Path
from typing import Optional, Dict

# Datasource
from datasource.env.env_reader import EnvReader
from datasource.database import Database
from datasource.file_system.file_config_repository import FileConfigRepository
from datasource.file_system.local_file_system_excluded_files_repository import LocalFileSystemExcludedFilesRepository
from datasource.sqlite.sqlite_user_repository import SqliteUserRepository
from datasource.sqlite.sqlite_export_config_repository import SqliteExportConfigRepository
from datasource.sqlite.sqlite_comparison_repository import SqliteComparisonRepository

# Infrastructure
from infrastructure.os.systemd_service_controller import SystemdServiceController
from infrastructure.os.signal_sender import SignalSender
from infrastructure.network.prometheus_health_check_client import PrometheusHealthCheckClient
from infrastructure.security.hmac_token_provider import HmacTokenProvider
from infrastructure.file_system.file_system_operator import LocalFileSystemOperator
from infrastructure.network.webhook_transport_gateway import WebhookTransportGateway
from infrastructure.network.rabbitmq_transport_gateway import RabbitMQTransportGateway
from infrastructure.scheduler.background_scheduler import BackgroundScheduler

# Domain
from domain.services.config_validator import ConfigValidator
from domain.services.jwt_provider import JwtTokenProvider
from domain.services.source_config_extractor import SourceConfigExtractor
from domain.services.comparison_version_validator import ComparisonVersionValidator
from domain.ports.transport_gateway import TransportGateway

# Application (Use Cases)
from application.use_cases.authenticate_user import AuthenticateUserUseCase
from application.use_cases.get_configuration import GetConfigurationUseCase
from application.use_cases.update_configuration import UpdateConfigurationUseCase
from application.use_cases.get_service_status import GetServiceStatusUseCase
from application.use_cases.restart_service import RestartServiceUseCase
from application.use_cases.generate_export_manifest import GenerateExportManifestUseCase
from application.use_cases.serve_file_by_token import ServeFileByTokenUseCase
from application.use_cases.process_acknowledgment import ProcessAcknowledgmentUseCase
from application.use_cases.dispatch_excluded_files import DispatchExcludedFilesUseCase
from application.use_cases.apply_comparison_batch import ApplyComparisonBatchUseCase
from application.use_cases.sync_comparison_list import SyncComparisonListUseCase
from application.use_cases.get_comparison_records import GetComparisonRecordsUseCase
from application.use_cases.update_comparison_list import UpdateComparisonListOrchestrator

from web.controller.comparison_controller import ComparisonController, comparison_bp
from application.use_cases.get_comparison_records import GetComparisonRecordsUseCase


logger = logging.getLogger(__name__)

class Container:
    """
    Корень композиции (Composition Root).
    Инкапсулирует создание и связывание всех зависимостей приложения.
    """

    def __init__(self, env_file: Optional[Path] = None):
        # 1. Загрузка переменных окружения
        self._env_reader = EnvReader(env_file or Path('.env'))
        self._env_reader.load()

        # 2. Инициализация персистентного хранилища (SQLite)
        db_path = Path(self._env_reader.get('DATABASE_PATH', './data/app.db'))
        self._db = Database(db_path)

        # 3. Инициализация репозиториев (Datasource)
        self._config_repo = FileConfigRepository(
            config_path=Path(self._env_reader.get('CONFIG_PATH')),
            backup_dir=Path(self._env_reader.get('BACKUP_DIR'))
        )
        self._user_repo = SqliteUserRepository(self._db)
        self._export_config_repo = SqliteExportConfigRepository(self._db)
        self._excluded_files_repo = LocalFileSystemExcludedFilesRepository()
        self._comparison_repo = SqliteComparisonRepository(self._db)

        # 4. Инициализация инфраструктурных адаптеров
        self._service_controller = SystemdServiceController(
            pid_file=Path(self._env_reader.get('SERVICE_PID_FILE')),
            service_name=self._env_reader.get('SERVICE_NAME', 'xmlfilter')
        )
        self._signal_sender = SignalSender(
            pid_file_path=Path(self._env_reader.get('SERVICE_PID_FILE'))
        )
        
        metrics_port = self._env_reader.get('SERVICE_METRICS_PORT', '9090')
        self._health_check_client = PrometheusHealthCheckClient(
            metrics_url=f"http://localhost:{metrics_port}/metrics"
        )
        
        self._token_provider = HmacTokenProvider(
            secret=self._env_reader.get('JWT_SECRET'),
            ttl_seconds=int(self._env_reader.get('EXPORT_TOKEN_TTL_SECONDS', '3600'))
        )
        
        self._fs_operator = LocalFileSystemOperator(
            trash_dir=self._env_reader.get('TRASH_DIR', '/opt/xmlfilter/.trash')
        )

        # Инициализация шлюзов транспорта (Transport Gateways)
        self._transport_gateways: Dict[str, TransportGateway] = {
            'webhook': WebhookTransportGateway(),
            'rabbitmq': RabbitMQTransportGateway()
        }

        # 5. Инициализация доменных сервисов
        schema_path = Path(self._env_reader.get('CONFIG_SCHEMA_PATH'))
        with open(schema_path, 'r', encoding='utf-8') as f:
            schema = json.load(f)
            
        self._config_validator = ConfigValidator(schema=schema)
        
        self._jwt_provider = JwtTokenProvider(
            secret=self._env_reader.get('JWT_SECRET'),
            algorithm=self._env_reader.get('JWT_ALGORITHM', 'HS256'),
            access_expires_minutes=int(self._env_reader.get('JWT_ACCESS_TOKEN_EXPIRES_MINUTES', '15')),
            refresh_expires_days=int(self._env_reader.get('JWT_REFRESH_TOKEN_EXPIRES_DAYS', '7'))
        )
        
        self._source_extractor = SourceConfigExtractor()
        self._comparison_validator = ComparisonVersionValidator(self._comparison_repo)

    # =========================================================================
    # Фабрики Use Cases (Application Layer)
    # =========================================================================

    def authenticate_user_use_case(self) -> AuthenticateUserUseCase:
        return AuthenticateUserUseCase(self._user_repo, self._jwt_provider)

    def get_configuration_use_case(self) -> GetConfigurationUseCase:
        return GetConfigurationUseCase(self._config_repo)

    def update_configuration_use_case(self) -> UpdateConfigurationUseCase:
        return UpdateConfigurationUseCase(
            config_repo=self._config_repo,
            config_validator=self._config_validator,
            service_controller=self._service_controller
        )

    def get_service_status_use_case(self) -> GetServiceStatusUseCase:
        return GetServiceStatusUseCase(self._service_controller)

    def restart_service_use_case(self) -> RestartServiceUseCase:
        return RestartServiceUseCase(self._service_controller)

    def generate_export_manifest_use_case(self) -> GenerateExportManifestUseCase:
        return GenerateExportManifestUseCase(
            config_repo=self._config_repo,
            source_extractor=self._source_extractor,
            excluded_files_repo=self._excluded_files_repo,
            token_provider=self._token_provider
        )

    def serve_file_by_token_use_case(self) -> ServeFileByTokenUseCase:
        return ServeFileByTokenUseCase(self._token_provider)

    def process_acknowledgment_use_case(self) -> ProcessAcknowledgmentUseCase:
        return ProcessAcknowledgmentUseCase(
            token_provider=self._token_provider,
            fs_operator=self._fs_operator,
            export_config_repo=self._export_config_repo
        )

    def dispatch_excluded_files_use_case(self) -> DispatchExcludedFilesUseCase:
        return DispatchExcludedFilesUseCase(
            config_repo=self._config_repo,
            source_extractor=self._source_extractor,
            excluded_files_repo=self._excluded_files_repo,
            export_config_repo=self._export_config_repo,
            transport_gateways=self._transport_gateways,
            fs_operator=self._fs_operator
        )

    def apply_comparison_batch_use_case(self) -> ApplyComparisonBatchUseCase:
        return ApplyComparisonBatchUseCase(self._comparison_repo, self._comparison_validator)

    def sync_comparison_list_use_case(self) -> SyncComparisonListUseCase:
        return SyncComparisonListUseCase(self._comparison_repo)

    def get_comparison_records_use_case(self) -> GetComparisonRecordsUseCase:
        return GetComparisonRecordsUseCase(self._comparison_repo)

    def update_comparison_list_orchestrator(self) -> UpdateComparisonListOrchestrator:
        return UpdateComparisonListOrchestrator(
            apply_batch_uc=self.apply_comparison_batch_use_case(),
            signal_sender=self._signal_sender
        )

    def background_scheduler(self) -> BackgroundScheduler:
        interval = int(self._env_reader.get('PUSH_DISPATCH_INTERVAL_SECONDS', '10'))
        return BackgroundScheduler(
            use_case=self.dispatch_excluded_files_use_case(),
            interval_seconds=interval
        )

    # =========================================================================
    # Прямой доступ к компонентам для Web/Middleware слоя
    # =========================================================================

    @property
    def jwt_provider(self) -> JwtTokenProvider:
        """Предоставляет провайдер JWT для middleware аутентификации."""
        return self._jwt_provider

    @property
    def export_config_repository(self) -> SqliteExportConfigRepository:
        """Предоставляет репозиторий профилей обмена для контроллеров."""
        return self._export_config_repo

    @property
    def comparison_repository(self) -> SqliteComparisonRepository:
        """Предоставляет репозиторий списков сравнения для контроллеров."""
        return self._comparison_repo

    # Фабрики Use Cases
    def get_comparison_records_use_case(self) -> GetComparisonRecordsUseCase:
        return GetComparisonRecordsUseCase(self._comparison_repo)

    # Прямой доступ для Web
    @property
    def comparison_repository(self) -> SqliteComparisonRepository:
        return self._comparison_repo