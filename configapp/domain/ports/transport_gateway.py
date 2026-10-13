from abc import ABC, abstractmethod
from domain.entities.excluded_file import ExcludedFile
from domain.entities.export_profile import ExportProfile

class TransportGateway(ABC):
    """Абстракция для отправки файлов во внешние системы (Push-модель)."""
    @abstractmethod
    def send(self, file: ExcludedFile, profile: ExportProfile) -> bool:
        """
        Отправляет файл или его метаданные согласно конфигурации профиля.
        Возвращает True в случае успешной доставки (или подтверждения брокером).
        """
        pass