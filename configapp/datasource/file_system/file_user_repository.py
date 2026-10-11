import json
import fcntl
import tempfile
import os
from pathlib import Path
from typing import Optional, List, Dict
from contextlib import contextmanager

from domain.ports.user_repository import UserRepository
from domain.entities.user import User

class FileUserRepository(UserRepository):
    """Реализация репозитория пользователей с файловым хранилищем и блокировками."""

    def __init__(self, users_file: Path):
        self._users_file = users_file
        self._users_file.parent.mkdir(parents=True, exist_ok=True)
        if not self._users_file.exists():
            self._write_users_raw([])

    def get_user(self, username: str) -> Optional[User]:
        with self._lock_file(shared=True):
            users = self._read_users_raw()
            for u_data in users:
                if u_data.get("username") == username:
                    return User(username=u_data["username"], password_hash=u_data["password_hash"])
        return None

    def save_user(self, user: User) -> None:
        with self._lock_file(shared=False):
            users = self._read_users_raw()
            updated = False
            for i, u_data in enumerate(users):
                if u_data.get("username") == user.username:
                    users[i] = {"username": user.username, "password_hash": user.password_hash}
                    updated = True
                    break
            if not updated:
                users.append({"username": user.username, "password_hash": user.password_hash})
            
            self._write_users_raw(users, locked=True)

    def _read_users_raw(self) -> List[Dict[str, str]]:
        try:
            with open(self._users_file, 'r', encoding='utf-8') as f:
                return json.load(f)
        except (json.JSONDecodeError, FileNotFoundError):
            return []

    def _write_users_raw(self, users: List[Dict[str, str]], locked: bool = False) -> None:
        fd, tmp_path = tempfile.mkstemp(
            prefix=self._users_file.name + ".", 
            suffix=".tmp", 
            dir=str(self._users_file.parent)
        )
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as f:
                json.dump(users, f, ensure_ascii=False, indent=2)
            os.replace(tmp_path, self._users_file)
        except Exception:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)
            raise

    @contextmanager
    def _lock_file(self, shared: bool):
        """Контекстный менеджер для файловой блокировки (fcntl)."""
        # Открываем файл для чтения/записи, чтобы получить файловый дескриптор для flock
        mode = 'r+' if self._users_file.exists() else 'w+'
        f = open(self._users_file, mode)
        try:
            lock_type = fcntl.LOCK_SH if shared else fcntl.LOCK_EX
            fcntl.flock(f.fileno(), lock_type)
            yield
        finally:
            fcntl.flock(f.fileno(), fcntl.LOCK_UN)
            f.close()