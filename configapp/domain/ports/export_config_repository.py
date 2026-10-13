from abc import ABC, abstractmethod
from typing import List, Optional
from domain.entities.export_profile import ExportProfile

class ExportConfigRepository(ABC):
    @abstractmethod
    def get_profile_by_source(self, source_name: str) -> Optional[ExportProfile]:
        pass

    @abstractmethod
    def get_all_profiles(self) -> List[ExportProfile]:
        pass

    @abstractmethod
    def save_profile(self, profile: ExportProfile) -> int:
        pass

    @abstractmethod
    def delete_profile(self, profile_id: int) -> None:
        pass