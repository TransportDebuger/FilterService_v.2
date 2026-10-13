from dataclasses import dataclass
from typing import Dict, Any, Optional
from domain.value_objects.exchange_action import ExchangeAction

@dataclass
class ExportProfile:
    """Профиль обмена данными для конкретного источника."""
    id: Optional[int]
    source_name: str
    transport_type: str  # 'pull', 'webhook', 'rabbitmq'
    transport_config: Dict[str, Any]  # URL, параметры брокера и т.д.
    ack_action: ExchangeAction