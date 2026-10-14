from domain.ports.comparison_repository import ComparisonRepository
from infrastructure.os.signal_sender import SignalSender
from domain.value_objects.comparison_dto import BatchUpdateRequest, UpdateResult
from application.use_cases.apply_comparison_batch import ApplyComparisonBatchUseCase
import logging

class UpdateComparisonListOrchestrator:
    """
    Оркестратор: применяет изменения и инициирует Hot-Reload в C++-сервисе.
    """
    def __init__(
        self, 
        apply_batch_uc: ApplyComparisonBatchUseCase, 
        signal_sender: SignalSender
    ):
        self._apply_batch_uc = apply_batch_uc
        self._signal_sender = signal_sender
        self._logger = logging.getLogger(__name__)

    def execute(self, request: BatchUpdateRequest) -> UpdateResult:
        # 1. Применение изменений в SQLite
        apply_result = self._apply_batch_uc.execute(request)
        
        if not apply_result.success:
            return apply_result

        # 2. Отправка сигнала SIGUSR1 (только если данные успешно сохранены)
        try:
            self._signal_sender.send_sigusr1()
            self._logger.info("SIGUSR1 sent to C++ service.")
        except Exception as e:
            self._logger.error(f"Failed to send SIGUSR1: {e}")
            # Данные уже сохранены, но сигнал не отправлен.
            # Возвращаем успех с предупреждением, чтобы внешняя система знала, что данные в БД.
            return UpdateResult(
                success=True, 
                new_version=apply_result.new_version, 
                error=f"Data saved, but SIGUSR1 failed: {str(e)}",
                data_persisted=True
            )

        return apply_result