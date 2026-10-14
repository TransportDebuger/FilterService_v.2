from flask import Blueprint, request, jsonify
from application.use_cases.update_comparison_list import UpdateComparisonListOrchestrator
from application.use_cases.get_comparison_records import GetComparisonRecordsUseCase
from domain.ports.comparison_repository import ComparisonRepository
from domain.value_objects.comparison_dto import BatchUpdateRequest, ComparisonOperation, OperationType

comparison_bp = Blueprint('comparison', __name__, url_prefix='/api/v1/comparison')

class ComparisonController:
    def __init__(
        self,
        orchestrator: UpdateComparisonListOrchestrator,
        get_records_uc: GetComparisonRecordsUseCase,
        comparison_repo: ComparisonRepository
    ):
        self._orchestrator = orchestrator
        self._get_records_uc = get_records_uc
        self._comparison_repo = comparison_repo

    def register_routes(self):
        @comparison_bp.route('/lists', methods=['GET'])
        def get_lists():
            lists = self._comparison_repo.get_all_lists()
            return jsonify([
                {"id": l.id, "db_path": l.db_path, "serial_version": l.serial_version}
                for l in lists
            ]), 200

        @comparison_bp.route('/records/<int:list_id>', methods=['GET'])
        def get_records(list_id):
            records = self._get_records_uc.execute(list_id)
            return jsonify([
                {"id": r.id, "list_id": r.list_id, "data": r.data, "record_version": r.record_version}
                for r in records
            ]), 200

        @comparison_bp.route('/version/<int:list_id>', methods=['GET'])
        def get_version(list_id):
            lst = self._comparison_repo.get_list_by_id(list_id)
            if not lst:
                return jsonify({"error": "List not found"}), 404
            return jsonify({"list_id": list_id, "serial_version": lst.serial_version}), 200

        @comparison_bp.route('/batch', methods=['POST'])
        def apply_batch():
            data = request.get_json()
            if not data or 'list_id' not in data or 'operations' not in data:
                return jsonify({"error": "Invalid payload"}), 400

            try:
                operations = []
                for op_data in data['operations']:
                    op_type = OperationType(op_data['op'])
                    operations.append(ComparisonOperation(
                        op=op_type,
                        data=op_data.get('data', {}),
                        record_id=op_data.get('record_id')
                    ))
                
                batch_request = BatchUpdateRequest(
                    list_id=data['list_id'],
                    version=data.get('version', 0),
                    operations=operations
                )
            except (ValueError, KeyError) as e:
                return jsonify({"error": f"Invalid operation format: {str(e)}"}), 400

            result = self._orchestrator.execute(batch_request)
            
            if result.success:
                return jsonify({
                    "status": "success",
                    "new_version": result.new_version,
                    "message": result.error or "Applied successfully"
                }), 200
            else:
                return jsonify({"status": "failed", "error": result.error}), 400