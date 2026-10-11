from flask import Blueprint, request, jsonify
from application.use_cases.get_configuration import GetConfigurationUseCase
from application.use_cases.update_configuration import UpdateConfigurationUseCase
from datasource.file_system.file_config_repository import ConfigConcurrencyError
from domain.services.config_validator import ConfigValidationError

config_bp = Blueprint('config', __name__, url_prefix='/api/v1/config')

class ConfigController:
    def __init__(self, get_config_use_case: GetConfigurationUseCase, update_config_use_case: UpdateConfigurationUseCase):
        self._get_config_use_case = get_config_use_case
        self._update_config_use_case = update_config_use_case

    def register_routes(self):
        @config_bp.route('', methods=['GET'])
        def get_config():
            dto = self._get_config_use_case.execute()
            response = jsonify(dto.config)
            response.headers['ETag'] = dto.etag
            return response, 200

        @config_bp.route('', methods=['PATCH'])
        def update_config():
            patch = request.get_json()
            if not isinstance(patch, list):
                return jsonify({"error": "Payload must be a JSON Patch array"}), 400
            
            base_etag = request.headers.get('If-Match')
            if not base_etag:
                return jsonify({"error": "If-Match header is required"}), 400
            
            try:
                dto = self._update_config_use_case.execute(patch, base_etag)
                response = jsonify(dto.config)
                response.headers['ETag'] = dto.etag
                return response, 200
            except ConfigConcurrencyError as e:
                return jsonify({"error": str(e)}), 409
            except ConfigValidationError as e:
                return jsonify({"error": str(e)}), 400
            except ValueError as e:
                return jsonify({"error": str(e)}), 409