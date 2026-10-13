import requests
from pathlib import Path
from domain.ports.transport_gateway import TransportGateway
from domain.entities.excluded_file import ExcludedFile
from domain.entities.export_profile import ExportProfile

class WebhookTransportGateway(TransportGateway):
    """Отправка файлов через HTTP POST (multipart/form-data)."""
    
    def send(self, file: ExcludedFile, profile: ExportProfile) -> bool:
        url = profile.transport_config.get('url')
        if not url:
            return False
        
        try:
            with open(file.file_path, 'rb') as f:
                files = {'file': (Path(file.file_path).name, f, 'application/xml')}
                data = {
                    'source_name': file.source_name, 
                    'file_hash': file.file_hash
                }
                response = requests.post(url, data=data, files=files, timeout=15)
                return 200 <= response.status_code < 300
        except requests.RequestException:
            return False