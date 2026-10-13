from abc import ABC, abstractmethod

class FileSystemOperator(ABC):
    @abstractmethod
    def delete_file(self, file_path: str) -> None:
        pass

    @abstractmethod
    def move_file(self, file_path: str, destination_dir: str) -> None:
        pass