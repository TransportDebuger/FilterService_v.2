from abc import ABC, abstractmethod

class HealthCheckClient(ABC):
    @abstractmethod
    def is_healthy(self, timeout_seconds: int = 5) -> bool:
        """
        Опрашивает эндпоинт метрик/статуса целевого сервиса.
        Возвращает True, если сервис отвечает успешно в рамках таймаута.
        """
        pass