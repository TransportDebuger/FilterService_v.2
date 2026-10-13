from dataclasses import dataclass
from typing import List

@dataclass(frozen=True)
class ExcludedFileDTO:
    file_path: str
    source_name: str
    size_bytes: int
    file_hash: str
    download_token: str
    ack_token: str

@dataclass(frozen=True)
class ExportManifestDTO:
    files: List[ExcludedFileDTO]
    total_count: int