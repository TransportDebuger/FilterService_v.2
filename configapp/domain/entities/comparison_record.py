from dataclasses import dataclass
from typing import Optional, Dict, Any

@dataclass
class ComparisonRecord:
    """Запись списка сравнения. Поле data хранит произвольный JSON."""
    id: Optional[int]
    list_id: int
    data: Dict[str, Any]  # Произвольный набор полей (например, {"lname": "Иванов", "ndoc": "123"})
    record_version: int
    is_deleted: bool = False