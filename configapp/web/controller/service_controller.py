from flask import Blueprint, jsonify
from application.use_cases.get_service_status import GetServiceStatusUseCase
from application.use_cases.restart_service import RestartServiceUseCase

service_bp = Blueprint('service', __name__, url_prefix='/api/v1/service')

class ServiceController:
    def __init__(self, get_status_use_case: GetServiceStatusUseCase, restart_use_case: RestartServiceUseCase):
        self._get_status_use_case = get_status_use_case
        self._restart_use_case = restart_use_case

    def register_routes(self):
        @service_bp.route('/status', methods=['GET'])
        def get_status():
            dto = self._get_status_use_case.execute()
            return jsonify({
                "is_active": dto.is_active,
                "pid": dto.pid,
                "status_text": dto.status_text
            }), 200

        @service_bp.route('/restart', methods=['POST'])
        def restart():
            try:
                self._restart_use_case.execute()
                return jsonify({"message": "Service restart initiated"}), 200
            except Exception as e:
                return jsonify({"error": str(e)}), 500