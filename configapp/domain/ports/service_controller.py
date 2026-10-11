from abc import ABC, abstractmethod
from domain.value_objects.service_state import ServiceState

class ServiceController(ABC):
    @abstractmethod
    def get_status(self) -> ServiceState:
        """Возвращает текущее состояние процесса сервиса."""
        pass

    @abstractmethod
    def reload(self) -> None:
        """Отправляет сигнал SIGHUP для перечитывания конфигурации без остановки."""
        pass

    @abstractmethod
    def restart(self) -> None:
        """Инициирует полную перезагрузку сервиса."""
        pass