import os
from pathlib import Path
from dotenv import load_dotenv

class EnvReader:
    """Инкапсулирует загрузку переменных окружения из .env файла."""
    
    def __init__(self, env_file: Path):
        self._env_file = env_file

    def load(self) -> None:
        """Загружает переменные окружения в os.environ."""
        if self._env_file.exists():
            load_dotenv(self._env_file, override=True)
        else:
            # Если .env нет, полагаемся на системные переменные окружения
            pass

    def get(self, key: str, default: str = None) -> str:
        return os.getenv(key, default)