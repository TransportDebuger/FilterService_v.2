import sqlite3
from pathlib import Path

class Database:
    """Инкапсулирует подключение и инициализацию схемы SQLite."""
    
    def __init__(self, db_path: Path):
        self._db_path = db_path
        self._init_db()

    def _init_db(self):
        self._db_path.parent.mkdir(parents=True, exist_ok=True)
        with sqlite3.connect(self._db_path) as conn:
            conn.execute("PRAGMA journal_mode=WAL;")
            conn.execute("""
                CREATE TABLE IF NOT EXISTS users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    username TEXT UNIQUE NOT NULL,
                    password_hash TEXT NOT NULL
                )
            """)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS export_profiles (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    source_name TEXT UNIQUE NOT NULL,
                    transport_type TEXT NOT NULL,
                    transport_config TEXT NOT NULL,
                    ack_action TEXT NOT NULL
                )
            """)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS comparison_lists (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    db_path TEXT UNIQUE NOT NULL,
                    serial_version INTEGER NOT NULL DEFAULT 1,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            """)
            
            conn.execute("""
                CREATE TABLE IF NOT EXISTS comparison_records (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    list_id INTEGER NOT NULL,
                    data TEXT NOT NULL, -- JSON строка
                    record_version INTEGER NOT NULL DEFAULT 1,
                    is_deleted BOOLEAN NOT NULL DEFAULT 0,
                    FOREIGN KEY(list_id) REFERENCES comparison_lists(id)
                )
            """)
            conn.commit()

    def get_connection(self):
        conn = sqlite3.connect(self._db_path)
        conn.execute("PRAGMA journal_mode=WAL;")
        return sqlite3.connect(self._db_path)