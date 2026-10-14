from dataclasses import dataclass
from typing import Optional
from datetime import datetime

@dataclass
class ComparisonList:
    """Метаданные списка сравнения (аналог зоны DNS)."""
    id: Optional[int]
    db_path: str  # Путь к SQLite-базе (используется для идентификации, хотя физически БД одна)
    serial_version: int  # Глобальный номер версии списка (SOA Serial)
    updated_at: datetime