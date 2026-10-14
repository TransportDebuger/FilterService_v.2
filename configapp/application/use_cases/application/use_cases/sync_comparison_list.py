from domain.ports.comparison_repository import ComparisonRepository
from domain.value_objects.comparison_dto import FullSyncRequest, UpdateResult
import logging

class SyncComparisonListUseCase:
    """
    Полная синхронизация (AXFR). Заменяет все активные записи на предоставленные.
    Используется при рассинхронизации версий или первичной инициализации.
    """
    def __init__(self, repo: ComparisonRepository):
        self._repo = repo
        self._logger = logging.getLogger(__name__)

    def execute(self, request: FullSyncRequest) -> UpdateResult:
        try:
            self._repo.begin_transaction()
            
            # Помечаем все текущие записи как удаленные (мягкое удаление)
            current_records = self._repo.get_records(request.list_id, include_deleted=False)
            for record in current_records:
                self._repo.delete_record(record.id)
            
            # Добавляем новые записи
            for data in request.records:
                self._repo.add_record(request.list_id, data)
                
            new_version = self._repo.increment_list_version(request.list_id)
            
            self._repo.commit()
            self._logger.info(f"Full sync completed. List {request.list_id}, new version {new_version}")
            return UpdateResult(success=True, new_version=new_version, data_persisted=True)

        except Exception as e:
            self._repo.rollback()
            self._logger.error(f"Full sync failed: {e}")
            return UpdateResult(success=False, error=str(e))