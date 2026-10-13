from domain.ports.token_provider import TokenProvider
from domain.ports.file_system_operator import FileSystemOperator
from domain.ports.export_config_repository import ExportConfigRepository
from domain.value_objects.exchange_action import ExchangeAction

class ProcessAcknowledgmentUseCase:
    def __init__(
        self, 
        token_provider: TokenProvider, 
        fs_operator: FileSystemOperator,
        export_config_repo: ExportConfigRepository
    ):
        self._token_provider = token_provider
        self._fs_operator = fs_operator
        self._export_config_repo = export_config_repo

    def execute(self, token: str) -> None:
        payload = self._token_provider.validate_token(token)
        if payload.get("action") != "ack":
            raise ValueError("Invalid token action")
            
        file_path = payload.get("file_path")
        source_name = payload.get("source")
        
        if not file_path or not source_name:
            raise ValueError("Invalid payload")
            
        profile = self._export_config_repo.get_profile_by_source(source_name)
        
        # Если профиль не задан, по умолчанию удаляем файл, чтобы не засорять диск
        if not profile or profile.ack_action == ExchangeAction.DELETE:
            self._fs_operator.delete_file(file_path)
        elif profile.ack_action == ExchangeAction.MOVE:
            dest_dir = profile.transport_config.get("archive_dir", "/opt/xmlfilter/archive")
            self._fs_operator.move_file(file_path, dest_dir)