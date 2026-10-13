from dataclasses import dataclass
import pathlib

@dataclass(frozen=True)
class SourceConfig:
    """
    Read-only представление источника данных, извлеченное из config.json.
    Инкапсулирует логику генерации маски для исключенных файлов.
    """
    name: str
    path: str
    excluded_dir: str
    file_mask: str
    excluded_template: str

    def get_excluded_mask(self) -> str:
        """
        Формирует glob-маску для поиска исключенных файлов.
        Основана на прямой подстановке file_mask в excluded_template,
        что полностью дублирует логику C++-сервиса (SourceConfig::applyTemplate).
        """
        p = pathlib.PurePath(self.file_mask)
        stem = p.stem
        ext = p.suffix.lstrip('.')
        
        mask = self.excluded_template
        mask = mask.replace("{filename}", stem)
        mask = mask.replace("{ext}", ext)
        
        return mask