from dataclasses import dataclass
from typing import List, Dict, Any, Optional
from enum import Enum

class OperationType(str, Enum):
    ADD = "add"
    UPDATE = "update"
    DELETE = "delete"

@dataclass
class ComparisonOperation:
    op: OperationType
    data: Dict[str, Any]
    record_id: Optional[int] = None  # Обязателен для UPDATE и DELETE

@dataclass
class BatchUpdateRequest:
    list_id: int
    version: int
    operations: List[ComparisonOperation]

@dataclass
class FullSyncRequest:
    list_id: int
    version: int
    records: List[Dict[str, Any]]

@dataclass
class UpdateResult:
    success: bool
    new_version: Optional[int] = None
    error: Optional[str] = None
    data_persisted: bool = False