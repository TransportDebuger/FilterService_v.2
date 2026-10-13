from abc import ABC, abstractmethod
from typing import List
from domain.entities.source_config import SourceConfig
from domain.entities.excluded_file import ExcludedFile

class ExcludedFilesRepository(ABC):
    @abstractmethod
    def find_excluded_files(self, sources: List[SourceConfig]) -> List[ExcludedFile]:
        pass