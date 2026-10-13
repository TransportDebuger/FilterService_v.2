from typing import Dict
from domain.ports.config_repository import ConfigRepository
from domain.ports.excluded_files_repository import ExcludedFilesRepository
from domain.ports.export_config_repository import ExportConfigRepository
from domain.ports.transport_gateway import TransportGateway
from domain.ports.file_system_operator import FileSystemOperator
from domain.services.source_config_extractor import SourceConfigExtractor
from domain.value_objects.exchange_action import ExchangeAction

class DispatchExcludedFilesUseCase:
    """
    Сканирует директории, находит файлы, для которых настроен Push-транспорт,
    отправляет их и применяет политику пост-обработки (удаление/перемещение).
    """
    def __init__(
        self,
        config_repo: ConfigRepository,
        source_extractor: SourceConfigExtractor,
        excluded_files_repo: ExcludedFilesRepository,
        export_config_repo: ExportConfigRepository,
        transport_gateways: Dict[str, TransportGateway],
        fs_operator: FileSystemOperator
    ):
        self._config_repo = config_repo
        self._source_extractor = source_extractor
        self._excluded_files_repo = excluded_files_repo
        self._export_config_repo = export_config_repo
        self._transport_gateways = transport_gateways
        self._fs_operator = fs_operator

    def execute(self) -> int:
        raw_config, _ = self._config_repo.read_config()
        sources = self._source_extractor.extract(raw_config)
        excluded_files = self._excluded_files_repo.find_excluded_files(sources)
        
        dispatched_count = 0
        for ef in excluded_files:
            profile = self._export_config_repo.get_profile_by_source(ef.source_name)
            
            # Пропускаем файлы, для которых не настроен профиль или выбран Pull
            if not profile or profile.transport_type == 'pull':
                continue
                
            gateway = self._transport_gateways.get(profile.transport_type)
            if not gateway:
                continue
                
            # Попытка отправки
            if gateway.send(ef, profile):
                # Успешная отправка. Применяем политику post-обработки.
                if profile.ack_action == ExchangeAction.DELETE:
                    self._fs_operator.delete_file(ef.file_path)
                elif profile.ack_action == ExchangeAction.MOVE:
                    dest = profile.transport_config.get('archive_dir', '/opt/xmlfilter/archive')
                    self._fs_operator.move_file(ef.file_path, dest)
                dispatched_count += 1
                
        return dispatched_count