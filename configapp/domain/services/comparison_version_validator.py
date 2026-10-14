from domain.ports.comparison_repository import ComparisonRepository

class ComparisonVersionValidator:
    def __init__(self, repo: ComparisonRepository):
        self._repo = repo

    def validate(self, list_id: int, requested_version: int) -> tuple[bool, str]:
        current_list = self._repo.get_list_by_id(list_id)
        if not current_list:
            return False, f"List with id {list_id} not found"
            
        current_version = current_list.serial_version
        
        if requested_version < current_version:
            return False, f"Stale version: requested {requested_version}, current {current_version}"
        
        if requested_version == current_version:
            return False, f"Duplicate version: {requested_version}"
        
        if requested_version > current_version + 1:
            return False, f"Version gap: requested {requested_version}, current {current_version}. Full sync required."
        
        return True, ""