from typing import List
from domain.ports.comparison_repository import ComparisonRepository
from domain.entities.comparison_record import ComparisonRecord

class GetComparisonRecordsUseCase:
    def __init__(self, repo: ComparisonRepository):
        self._repo = repo

    def execute(self, list_id: int) -> List[ComparisonRecord]:
        return self._repo.get_records(list_id, include_deleted=False)