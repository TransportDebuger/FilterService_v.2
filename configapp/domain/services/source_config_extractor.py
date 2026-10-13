from typing import List, Dict, Any
from domain.entities.source_config import SourceConfig

class SourceConfigExtractor:
    """
    Извлекает плоский список конфигураций источников из иерархического JSON.
    Учитывает как глобальные настройки (defaults), так и переопределения в окружениях.
    """
    def extract(self, raw_config: Dict[str, Any]) -> List[SourceConfig]:
        sources = []
        
        defaults = raw_config.get("defaults", {})
        for s in defaults.get("sources", []):
            sources.append(self._map_to_entity(s))
            
        environments = raw_config.get("environments", {})
        for env_name, env_data in environments.items():
            for s in env_data.get("sources", []):
                sources.append(self._map_to_entity(s, env_name))
                
        return sources

    def _map_to_entity(self, data: Dict[str, Any], env_name: str = None) -> SourceConfig:
        return SourceConfig(
            name=data.get("name"),
            path=data.get("path", ""),
            excluded_dir=data.get("excluded_dir", ""),
            file_mask=data.get("file_mask", "*.xml"),
            excluded_template=data.get("excluded_template", "{filename}_excluded.{ext}")
        )