from domain.ports.config_repository import ConfigRepository
from application.dto.config_dto import ConfigResponseDTO

class GetConfigurationUseCase:
    def __init__(self, config_repo: ConfigRepository):
        self._config_repo = config_repo

    def execute(self) -> ConfigResponseDTO:
        config, etag = self._config_repo.read_config()
        return ConfigResponseDTO(config=config, etag=str(etag))