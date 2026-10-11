import requests
from domain.ports.health_check_client import HealthCheckClient

class PrometheusHealthCheckClient(HealthCheckClient):
    """Проверка живости сервиса через опрос эндпоинта метрик."""

    def __init__(self, metrics_url: str):
        self._metrics_url = metrics_url

    def is_healthy(self, timeout_seconds: int = 5) -> bool:
        try:
            response = requests.get(self._metrics_url, timeout=timeout_seconds)
            return response.status_code == 200
        except requests.RequestException:
            return False