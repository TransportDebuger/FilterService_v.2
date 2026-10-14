import os
import signal
from pathlib import Path

class SignalSender:
    def __init__(self, pid_file_path: Path):
        self._pid_file_path = pid_file_path

    def send_sigusr1(self) -> None:
        if not self._pid_file_path.exists():
            raise FileNotFoundError(f"PID file not found: {self._pid_file_path}")
        
        with open(self._pid_file_path, 'r') as f:
            pid_str = f.read().strip()
            
        if not pid_str:
            raise ValueError("PID file is empty")
            
        pid = int(pid_str)
        os.kill(pid, signal.SIGUSR1)