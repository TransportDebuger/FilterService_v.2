from domain.ports.comparison_repository import ComparisonRepository
from domain.services.comparison_version_validator import ComparisonVersionValidator
from domain.value_objects.comparison_dto import BatchUpdateRequest, UpdateResult
import logging

class ApplyComparisonBatchUseCase:
    def __init__(self, repo: ComparisonRepository, validator: ComparisonVersionValidator):
        self._repo = repo
        self._validator = validator
        self._logger = logging.getLogger(__name__)

    def execute(self, request: BatchUpdateRequest) -> UpdateResult:
        # 1. Валидация версии
        is_valid, error_msg = self._validator.validate(request.list_id, request.version)
        if not is_valid:
            return UpdateResult(success=False, error=error_msg)

        # 2. Применение пакета в транзакции
        try:
            self._repo.begin_transaction()
            
            for op in request.operations:
                if op.op == "add":
                    self._repo.add_record(request.list_id, op.data)
                elif op.op == "update":
                    if op.record_id is None:
                        raise ValueError("record_id is required for UPDATE operation")
                    self._repo.update_record(op.record_id, op.data)
                elif op.op == "delete":
                    if op.record_id is None:
                        raise ValueError("record_id is required for DELETE operation")
                    self._repo.delete_record(op.record_id)
                else:
                    raise ValueError(f"Unknown operation type: {op.op}")

            # 3. Инкремент глобальной версии
            new_version = self._repo.increment_list_version(request.list_id)
            
            self._repo.commit()
            self._logger.info(f"Batch applied successfully. List {request.list_id}, new version {new_version}")
            return UpdateResult(success=True, new_version=new_version, data_persisted=True)

        except Exception as e:
            self._repo.rollback()
            self._logger.error(f"Batch apply failed: {e}")
            return UpdateResult(success=False, error=str(e), data_persisted=False)