import json
from typing import List, Optional
from domain.ports.export_config_repository import ExportConfigRepository
from domain.entities.export_profile import ExportProfile
from domain.value_objects.exchange_action import ExchangeAction
from datasource.database import Database

class SqliteExportConfigRepository(ExportConfigRepository):
    def __init__(self, db: Database):
        self._db = db

    def get_profile_by_source(self, source_name: str) -> Optional[ExportProfile]:
        with self._db.get_connection() as conn:
            cursor = conn.execute(
                "SELECT id, source_name, transport_type, transport_config, ack_action FROM export_profiles WHERE source_name = ?",
                (source_name,)
            )
            row = cursor.fetchone()
            return self._map_row_to_profile(row) if row else None

    def get_all_profiles(self) -> List[ExportProfile]:
        with self._db.get_connection() as conn:
            cursor = conn.execute(
                "SELECT id, source_name, transport_type, transport_config, ack_action FROM export_profiles"
            )
            return [self._map_row_to_profile(row) for row in cursor.fetchall()]

    def save_profile(self, profile: ExportProfile) -> int:
        config_json = json.dumps(profile.transport_config)
        with self._db.get_connection() as conn:
            if profile.id:
                conn.execute("""
                    UPDATE export_profiles 
                    SET source_name=?, transport_type=?, transport_config=?, ack_action=?
                    WHERE id=?
                """, (profile.source_name, profile.transport_type, config_json, profile.ack_action.value, profile.id))
                conn.commit()
                return profile.id
            else:
                cursor = conn.execute("""
                    INSERT INTO export_profiles (source_name, transport_type, transport_config, ack_action)
                    VALUES (?, ?, ?, ?)
                """, (profile.source_name, profile.transport_type, config_json, profile.ack_action.value))
                conn.commit()
                return cursor.lastrowid

    def delete_profile(self, profile_id: int) -> None:
        with self._db.get_connection() as conn:
            conn.execute("DELETE FROM export_profiles WHERE id = ?", (profile_id,))
            conn.commit()

    def _map_row_to_profile(self, row) -> ExportProfile:
        return ExportProfile(
            id=row[0],
            source_name=row[1],
            transport_type=row[2],
            transport_config=json.loads(row[3]),
            ack_action=ExchangeAction(row[4])
        )