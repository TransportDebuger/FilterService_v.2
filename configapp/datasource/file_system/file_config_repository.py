import json
import hashlib
import os
import tempfile
import shutil
from pathlib import Path
from datetime import datetime
from typing import Tuple, Dict, Any

from domain.ports.config_repository import ConfigRepository
from domain.value_objects.etag import ETag

class ConfigConcurrencyError(Exception):
    """Исключение при конфликте версий (Optimistic Concurrency Control)."""
    pass

class FileConfigRepository(ConfigRepository):
    """Реализация репозитория конфигурации с атомарной записью и бэкапами."""

    def __init__(self, config_path: Path, backup_dir: Path):
        self._config_path = config_path
        self._backup_dir = backup_dir
        self._backup_dir.mkdir(parents=True, exist_ok=True)

    def read_config(self) -> Tuple[Dict[str, Any], ETag]:
        if not self._config_path.exists():
            return {}, ETag(self._compute_etag({}))
        
        with open(self._config_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        etag = self._compute_etag(data)
        return data, ETag(etag)

    def write_config(self, config: Dict[str, Any], expected_etag: ETag) -> ETag:
        # 1. Проверка конкурентного доступа (Optimistic Locking)
        current_data, current_etag = self.read_config()
        if str(current_etag) != str(expected_etag):
            raise ConfigConcurrencyError(
                f"ETag mismatch. Expected: {expected_etag}, Current: {current_etag}"
            )

        # 2. Создание резервной копии
        if self._config_path.exists():
            timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            backup_name = f"{self._config_path.stem}-{timestamp}{self._config_path.suffix}"
            shutil.copy2(self._config_path, self._backup_dir / backup_name)

        # 3. Атомарная запись
        self._config_path.parent.mkdir(parents=True, exist_ok=True)
        fd, tmp_path = tempfile.mkstemp(
            prefix=self._config_path.name + ".", 
            suffix=".tmp", 
            dir=str(self._config_path.parent)
        )
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as f:
                json.dump(config, f, ensure_ascii=False, indent=2)
                f.write("\n")
            os.replace(tmp_path, self._config_path)
        except Exception:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)
            raise

        # 4. Возврат нового ETag
        new_etag = self._compute_etag(config)
        return ETag(new_etag)

    def _compute_etag(self, data: Dict[str, Any]) -> str:
        raw = json.dumps(data, sort_keys=True, ensure_ascii=False).encode('utf-8')
        return hashlib.sha256(raw).hexdigest()