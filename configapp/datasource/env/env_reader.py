import json
import os
from pathlib import Path
from dotenv import load_dotenv

class EnvReader:
    """
    Загрузчик конфигурации окружения.
    Реализует гибридный механизм: db_config.json имеет приоритет над .env
    для параметров персистентности (путь к SQLite).
    """
    def __init__(self, env_file: Path, db_config_file: Path = None):
        self._env_file = env_file
        self._db_config_file = db_config_file or Path('/opt/xmlfilter/db_config.json')

    def load(self):
        # 1. Загрузка базовых переменных из .env
        if self._env_file.exists():
            load_dotenv(self._env_file, override=True)
        
        # 2. Переопределение путей к БД из db_config.json (Единый источник истины)
        if self._db_config_file.exists():
            with open(self._db_config_file, 'r', encoding='utf-8') as f:
                db_config = json.load(f)
                if 'sqlite_db_path' in db_config:
                    os.environ['DATABASE_PATH'] = db_config['sqlite_db_path']

    def get(self, key: str, default: str = None) -> str:
        return os.getenv(key, default)