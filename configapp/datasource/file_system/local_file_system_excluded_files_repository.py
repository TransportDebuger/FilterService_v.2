import hashlib
import fnmatch
from pathlib import Path
from typing import List
from domain.ports.excluded_files_repository import ExcludedFilesRepository
from domain.entities.excluded_file import ExcludedFile
from domain.entities.source_config import SourceConfig

class LocalFileSystemExcludedFilesRepository(ExcludedFilesRepository):
    def find_excluded_files(self, sources: List[SourceConfig]) -> List[ExcludedFile]:
        excluded_files = []
        
        for source in sources:
            source_dir = Path(source.excluded_dir)
            if not source_dir.exists() or not source_dir.is_dir():
                continue
                
            # Генерация маски на основе прямой подстановки (дублирует логику C++-сервиса)
            mask = source.get_excluded_mask()
            
            for file_path in source_dir.rglob('*'):
                if file_path.is_file() and fnmatch.fnmatch(file_path.name, mask):
                    file_hash = self._compute_hash(file_path)
                    excluded_files.append(ExcludedFile(
                        file_path=str(file_path),
                        source_name=source.name,
                        size_bytes=file_path.stat().st_size,
                        file_hash=file_hash
                    ))
                    
        return excluded_files

    def _compute_hash(self, file_path: Path) -> str:
        sha256 = hashlib.sha256()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256.update(chunk)
        return sha256.hexdigest()