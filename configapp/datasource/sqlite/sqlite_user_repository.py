from typing import Optional
from domain.ports.user_repository import UserRepository
from domain.entities.user import User
from datasource.database import Database

class SqliteUserRepository(UserRepository):
    def __init__(self, db: Database):
        self._db = db

    def get_user(self, username: str) -> Optional[User]:
        with self._db.get_connection() as conn:
            cursor = conn.execute(
                "SELECT username, password_hash FROM users WHERE username = ?", 
                (username,)
            )
            row = cursor.fetchone()
            if row:
                return User(username=row[0], password_hash=row[1])
        return None

    def save_user(self, user: User) -> None:
        with self._db.get_connection() as conn:
            conn.execute("""
                INSERT INTO users (username, password_hash) VALUES (?, ?)
                ON CONFLICT(username) DO UPDATE SET password_hash = excluded.password_hash
            """, (user.username, user.password_hash))
            conn.commit()