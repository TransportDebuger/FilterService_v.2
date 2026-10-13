from flask import Blueprint, request, jsonify, send_file
from application.use_cases.generate_export_manifest import GenerateExportManifestUseCase
from application.use_cases.serve_file_by_token import ServeFileByTokenUseCase
from application.use_cases.process_acknowledgment import ProcessAcknowledgmentUseCase
from domain.ports.export_config_repository import ExportConfigRepository
from domain.entities.export_profile import ExportProfile
from domain.value_objects.exchange_action import ExchangeAction

export_bp = Blueprint('export', __name__, url_prefix='/api/v1/export')

class ExportController:
    def __init__(
        self,
        generate_manifest_uc: GenerateExportManifestUseCase,
        serve_file_uc: ServeFileByTokenUseCase,
        process_ack_uc: ProcessAcknowledgmentUseCase,
        export_config_repo: ExportConfigRepository
    ):
        self._generate_manifest_uc = generate_manifest_uc
        self._serve_file_uc = serve_file_uc
        self._process_ack_uc = process_ack_uc
        self._export_config_repo = export_config_repo

    def register_routes(self):
        # =====================================================================
        # Pull-модель: Манифест, Скачивание, Подтверждение (ACK)
        # =====================================================================

        @export_bp.route('/manifest', methods=['GET'])
        def get_manifest():
            """
            Возвращает список исключенных файлов и токены для их скачивания/ACK.
            Требует JWT-аутентификации.
            """
            dto = self._generate_manifest_uc.execute()
            files_data = [
                {
                    "file_path": f.file_path,
                    "source_name": f.source_name,
                    "size_bytes": f.size_bytes,
                    "file_hash": f.file_hash,
                    "download_token": f.download_token,
                    "ack_token": f.ack_token
                }
                for f in dto.files
            ]
            return jsonify({"files": files_data, "total_count": dto.total_count}), 200

        @export_bp.route('/download', methods=['GET'])
        def download_file():
            """
            Отдает файл по HMAC-токену.
            Не требует JWT-аутентификации (защищен самим фактом валидности токена).
            """
            token = request.args.get('token')
            if not token:
                return jsonify({"error": "Query parameter 'token' is required"}), 400
            
            try:
                file_path = self._serve_file_uc.execute(token)
                return send_file(file_path, as_attachment=True)
            except ValueError as e:
                return jsonify({"error": str(e)}), 401
            except FileNotFoundError:
                return jsonify({"error": "File not found or already processed"}), 404

        @export_bp.route('/ack', methods=['POST'])
        def acknowledge():
            """
            Подтверждение получения файла. Инициирует физическое удаление или перемещение.
            Не требует JWT-аутентификации.
            """
            token = None
            if request.is_json and 'token' in request.json:
                token = request.json['token']
            else:
                token = request.headers.get('X-ACK-Token')
            
            if not token:
                return jsonify({"error": "Token is required (in JSON body or X-ACK-Token header)"}), 400
                
            try:
                self._process_ack_uc.execute(token)
                return jsonify({"message": "Acknowledged successfully"}), 200
            except ValueError as e:
                return jsonify({"error": str(e)}), 401

        # =====================================================================
        # Управление профилями обмена (CRUD)
        # =====================================================================

        @export_bp.route('/profiles', methods=['GET'])
        def get_profiles():
            """Возвращает все настроенные профили обмена. Требует JWT."""
            profiles = self._export_config_repo.get_all_profiles()
            profiles_data = [
                {
                    "id": p.id,
                    "source_name": p.source_name,
                    "transport_type": p.transport_type,
                    "transport_config": p.transport_config,
                    "ack_action": p.ack_action.value
                }
                for p in profiles
            ]
            return jsonify(profiles_data), 200

        @export_bp.route('/profiles', methods=['POST'])
        def save_profile():
            """Создает или обновляет профиль обмена. Требует JWT."""
            data = request.get_json()
            if not data:
                return jsonify({"error": "Invalid JSON payload"}), 400
            
            try:
                action = ExchangeAction(data.get('ack_action', 'delete'))
            except ValueError:
                return jsonify({"error": f"Invalid ack_action. Allowed: {[e.value for e in ExchangeAction]}"}), 400

            profile = ExportProfile(
                id=data.get('id'),
                source_name=data.get('source_name'),
                transport_type=data.get('transport_type'),
                transport_config=data.get('transport_config', {}),
                ack_action=action
            )
            
            if not profile.source_name or not profile.transport_type:
                return jsonify({"error": "Fields 'source_name' and 'transport_type' are required"}), 400

            profile_id = self._export_config_repo.save_profile(profile)
            return jsonify({"id": profile_id, "message": "Profile saved successfully"}), 200

        @export_bp.route('/profiles/<int:profile_id>', methods=['DELETE'])
        def delete_profile(profile_id):
            """Удаляет профиль обмена. Требует JWT."""
            self._export_config_repo.delete_profile(profile_id)
            return jsonify({"message": "Profile deleted successfully"}), 200