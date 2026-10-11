from abc import ABC, abstractmethod
from typing import Any, Dict, Tuple
from domain.value_objects.etag import ETag

class ConfigRepository(ABC):
    @abstractmethod
    def read_config(self) -> Tuple[Dict[str, Any], ETag]:
        """Читает конфигурацию и возвращает её текущий ETag."""
        pass

    @abstractmethod
    def write_config(self, config: Dict[str, Any], expected_etag: ETag) -> ETag:
        """
        Атомарно записывает конфигурацию.
        Выбрасывает исключение при несовпадении expected_etag с актуальным (Optimistic Concurrency Control).
        Возвращает новый ETag записанной конфигурации.
        """
        pass