from dataclasses import dataclass

@dataclass(frozen=True)
class ExcludedFile:
    """Метаданные файла, исключенного C++-сервисом."""
    file_path: str
    source_name: str
    size_bytes: int
    file_hash: str