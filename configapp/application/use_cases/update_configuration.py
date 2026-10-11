import jsonpatch
from typing import List, Dict, Any
from domain.ports.config_repository import ConfigRepository
from domain.ports.service_controller import ServiceController
from domain.services.config_validator import ConfigValidator
from application.dto.config_dto import ConfigResponseDTO

class UpdateConfigurationUseCase:
    def __init__(
        self, 
        config_repo: ConfigRepository, 
        config_validator: ConfigValidator,
        service_controller: ServiceController
    ):
        self._config_repo = config_repo
        self._config_validator = config_validator
        self._service_controller = service_controller

    def execute(self, patch: List[Dict[str, Any]], base_etag: str) -> ConfigResponseDTO:
        # 1. Чтение текущей конфигурации и проверка ETag
        current_config, current_etag = self._config_repo.read_config()
        if str(current_etag) != base_etag:
            raise ValueError("Configuration has been modified by another user (ETag mismatch)")

        # 2. Применение JSON Patch
        patch_obj = jsonpatch.JsonPatch(patch)
        new_config = patch_obj.apply(current_config)

        # 3. Валидация результирующей конфигурации
        self._config_validator.validate(new_config)

        # 4. Атомарная запись
        new_etag = self._config_repo.write_config(new_config, current_etag)

        # 5. Перезагрузка сервиса (отправка SIGHUP)
        try:
            self._service_controller.reload()
        except Exception as e:
            # Логирование ошибки перезагрузки, но конфигурация уже сохранена
            pass 

        return ConfigResponseDTO(config=new_config, etag=str(new_etag))