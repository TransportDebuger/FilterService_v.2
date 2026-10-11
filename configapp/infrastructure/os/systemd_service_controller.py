import os
import signal
import subprocess
from pathlib import Path
from typing import Optional

from domain.ports.service_controller import ServiceController
from domain.value_objects.service_state import ServiceState

class SystemdServiceController(ServiceController):
    """Управление жизненным циклом сервиса через systemd и прямую отправку сигналов."""

    def __init__(self, pid_file: Path, service_name: str):
        self._pid_file = pid_file
        self._service_name = service_name

    def get_status(self) -> ServiceState:
        pid = self._read_pid()
        if pid is None:
            return ServiceState(is_active=False, pid=None, status_text="inactive (no pid file)")
        
        if self._is_pid_alive(pid):
            return ServiceState(is_active=True, pid=pid, status_text="active")
        else:
            return ServiceState(is_active=False, pid=pid, status_text="failed (stale pid file)")

    def reload(self) -> None:
        """Отправляет SIGHUP для транзакционной перезагрузки конфигурации."""
        pid = self._read_pid()
        if pid is None or not self._is_pid_alive(pid):
            raise RuntimeError("Service is not running. Cannot send SIGHUP.")
        
        os.kill(pid, signal.SIGHUP)

    def restart(self) -> None:
        """Инициирует полную перезагрузку через systemctl."""
        result = subprocess.run(
            ['systemctl', 'restart', self._service_name],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            raise RuntimeError(f"systemctl restart failed: {result.stderr}")

    def _read_pid(self) -> Optional[int]:
        if not self._pid_file.exists():
            return None
        try:
            with open(self._pid_file, 'r') as f:
                return int(f.read().strip())
        except (ValueError, IOError):
            return None

    def _is_pid_alive(self, pid: int) -> bool:
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False