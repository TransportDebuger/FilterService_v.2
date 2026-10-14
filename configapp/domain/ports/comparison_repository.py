from abc import ABC, abstractmethod
from typing import List, Optional
from domain.entities.comparison_list import ComparisonList
from domain.entities.comparison_record import ComparisonRecord

class ComparisonRepository(ABC):
    @abstractmethod
    def get_list_by_id(self, list_id: int) -> Optional[ComparisonList]:
        pass

    @abstractmethod
    def get_records(self, list_id: int, include_deleted: bool = False) -> List[ComparisonRecord]:
        pass

    @abstractmethod
    def add_record(self, list_id: int, data: dict) -> int:
        pass

    @abstractmethod
    def update_record(self, record_id: int, new_data: dict) -> None:
        pass

    @abstractmethod
    def delete_record(self, record_id: int) -> None:
        pass

    @abstractmethod
    def increment_list_version(self, list_id: int) -> int:
        pass

    @abstractmethod
    def begin_transaction(self) -> None:
        pass

    @abstractmethod
    def commit(self) -> None:
        pass

    @abstractmethod
    def rollback(self) -> None:
        pass
    
    @abstractmethod
    def get_or_create_list(self, db_path: str) -> int:
        """Возвращает ID списка. Если список не существует, создает его с версией 1."""
        pass

    @abstractmethod
    def get_all_lists(self) -> List['ComparisonList']:
        pass