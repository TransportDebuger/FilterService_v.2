import threading
import logging

class BackgroundScheduler:
    """
    Инкапсулирует фоновый поток для периодического запуска Use Case.
    Использует threading.Event для корректного завершения (graceful shutdown).
    """
    def __init__(self, use_case, interval_seconds: int = 10):
        self._use_case = use_case
        self._interval = interval_seconds
        self._stop_event = threading.Event()
        self._thread = None
        self._logger = logging.getLogger(__name__)

    def start(self):
        if self._thread and self._thread.is_alive():
            return
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run, daemon=True, name="PushDispatcher")
        self._thread.start()
        self._logger.info(f"Background scheduler started (interval: {self._interval}s)")

    def stop(self):
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=5)
        self._logger.info("Background scheduler stopped")

    def _run(self):
        while not self._stop_event.is_set():
            try:
                count = self._use_case.execute()
                if count > 0:
                    self._logger.info(f"Push dispatcher processed {count} files")
            except Exception as e:
                self._logger.exception(f"Error in background dispatch loop: {e}")
            
            # Ожидание с возможностью прерывания через stop_event
            self._stop_event.wait(self._interval)