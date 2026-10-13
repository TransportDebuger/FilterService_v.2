from domain.ports.config_repository import ConfigRepository
from domain.ports.excluded_files_repository import ExcludedFilesRepository
from domain.ports.token_provider import TokenProvider
from domain.services.source_config_extractor import SourceConfigExtractor
from application.dto.export_dto import ExportManifestDTO, ExcludedFileDTO

class GenerateExportManifestUseCase:
    def __init__(
        self, 
        config_repo: ConfigRepository,
        source_extractor: SourceConfigExtractor,
        excluded_files_repo: ExcludedFilesRepository, 
        token_provider: TokenProvider
    ):
        self._config_repo = config_repo
        self._source_extractor = source_extractor
        self._repo = excluded_files_repo
        self._token_provider = token_provider

    def execute(self) -> ExportManifestDTO:
        raw_config, _ = self._config_repo.read_config()
        sources = self._source_extractor.extract(raw_config)
        
        excluded_files = self._repo.find_excluded_files(sources)
        
        manifest_files = []
        for ef in excluded_files:
            download_payload = {"action": "download", "file_path": ef.file_path, "hash": ef.file_hash}
            ack_payload = {"action": "ack", "file_path": ef.file_path, "hash": ef.file_hash, "source": ef.source_name}
            
            manifest_files.append(ExcludedFileDTO(
                file_path=ef.file_path,
                source_name=ef.source_name,
                size_bytes=ef.size_bytes,
                file_hash=ef.file_hash,
                download_token=self._token_provider.generate_token(download_payload),
                ack_token=self._token_provider.generate_token(ack_payload)
            ))
            
        return ExportManifestDTO(files=manifest_files, total_count=len(manifest_files))