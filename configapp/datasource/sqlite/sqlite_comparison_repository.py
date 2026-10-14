import json
from typing import List, Optional
from datetime import datetime
from domain.ports.comparison_repository import ComparisonRepository
from domain.entities.comparison_list import ComparisonList
from domain.entities.comparison_record import ComparisonRecord
from datasource.database import Database

class SqliteComparisonRepository(ComparisonRepository):
    def __init__(self, db: Database):
        self._db = db

    def get_list_by_id(self, list_id: int) -> Optional[ComparisonList]:
        with self._db.get_connection() as conn:
            cursor = conn.execute(
                "SELECT id, db_path, serial_version, updated_at FROM comparison_lists WHERE id = ?", 
                (list_id,)
            )
            row = cursor.fetchone()
            if row:
                return ComparisonList(
                    id=row[0], 
                    db_path=row[1], 
                    serial_version=row[2], 
                    updated_at=datetime.fromisoformat(row[3])
                )
        return None

    def get_records(self, list_id: int, include_deleted: bool = False) -> List[ComparisonRecord]:
        with self._db.get_connection() as conn:
            query = "SELECT id, list_id, data, record_version, is_deleted FROM comparison_records WHERE list_id = ?"
            if not include_deleted:
                query += " AND is_deleted = 0"
            
            cursor = conn.execute(query, (list_id,))
            records = []
            for row in cursor.fetchall():
                records.append(ComparisonRecord(
                    id=row[0],
                    list_id=row[1],
                    data=json.loads(row[2]),
                    record_version=row[3],
                    is_deleted=bool(row[4])
                ))
            return records

    def add_record(self, list_id: int, data: dict) -> int:
        with self._db.get_connection() as conn:
            cursor = conn.execute(
                "INSERT INTO comparison_records (list_id, data, record_version) VALUES (?, ?, 1)",
                (list_id, json.dumps(data, ensure_ascii=False))
            )
            conn.commit()
            return cursor.lastrowid

    def update_record(self, record_id: int, new_data: dict) -> None:
        with self._db.get_connection() as conn:
            conn.execute(
                "UPDATE comparison_records SET data = ?, record_version = record_version + 1 WHERE id = ?",
                (json.dumps(new_data, ensure_ascii=False), record_id)
            )
            conn.commit()

    def delete_record(self, record_id: int) -> None:
        with self._db.get_connection() as conn:
            conn.execute(
                "UPDATE comparison_records SET is_deleted = 1, record_version = record_version + 1 WHERE id = ?",
                (record_id,)
            )
            conn.commit()

    def increment_list_version(self, list_id: int) -> int:
        with self._db.get_connection() as conn:
            conn.execute(
                "UPDATE comparison_lists SET serial_version = serial_version + 1, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                (list_id,)
            )
            conn.commit()
            
            cursor = conn.execute("SELECT serial_version FROM comparison_lists WHERE id = ?", (list_id,))
            return cursor.fetchone()[0]

    def begin_transaction(self) -> None:
        self._conn = self._db.get_connection()
        self._conn.execute("BEGIN TRANSACTION")

    def commit(self) -> None:
        if self._conn:
            self._conn.commit()
            self._conn.close()
            self._conn = None

    def rollback(self) -> None:
        if self._conn:
            self._conn.rollback()
            self._conn.close()
            self._conn = None

    def get_or_create_list(self, db_path: str) -> int:
        with self._db.get_connection() as conn:
            cursor = conn.execute("SELECT id FROM comparison_lists WHERE db_path = ?", (db_path,))
            row = cursor.fetchone()
            if row:
                return row[0]
            
            cursor = conn.execute(
                "INSERT INTO comparison_lists (db_path, serial_version) VALUES (?, 1)", 
                (db_path,)
            )
            conn.commit()
            return cursor.lastrowid

    def get_all_lists(self) -> List[ComparisonList]:
        with self._db.get_connection() as conn:
            cursor = conn.execute("SELECT id, db_path, serial_version, updated_at FROM comparison_lists")
            return [
                ComparisonList(
                    id=row[0], db_path=row[1], serial_version=row[2], 
                    updated_at=datetime.fromisoformat(row[3])
                )
                for row in cursor.fetchall()
            ]    