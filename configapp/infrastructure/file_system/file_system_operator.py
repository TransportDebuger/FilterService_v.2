import os
import time
from pathlib import Path
from domain.ports.file_system_operator import FileSystemOperator

class LocalFileSystemOperator(FileSystemOperator):
    """
    Обеспечивает безопасное удаление и перемещение файлов.
    Удаление выполняется атомарно через предварительное перемещение в директорию-корзину (.trash).
    """
    def __init__(self, trash_dir: str = "/opt/xmlfilter/.trash"):
        self._trash_dir = Path(trash_dir)
        self._trash_dir.mkdir(parents=True, exist_ok=True)

    def delete_file(self, file_path: str) -> None:
        path = Path(file_path)
        if not path.exists():
            return
        
        trash_path = self._trash_dir / f"{path.name}.{os.getpid()}.{int(time.time())}"
        os.replace(path, trash_path)
        os.unlink(trash_path)

    def move_file(self, file_path: str, destination_dir: str) -> None:
        path = Path(file_path)
        if not path.exists():
            return
            
        dest = Path(destination_dir)
        dest.mkdir(parents=True, exist_ok=True)
        dest_path = dest / path.name
        os.replace(path, dest_path)