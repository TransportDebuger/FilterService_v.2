import logging
import atexit
from flask import Flask, jsonify, render_template
from di.container import Container
from web.controller.auth_controller import AuthController, auth_bp
from web.controller.config_controller import ConfigController, config_bp
from web.controller.service_controller import ServiceController, service_bp
from web.controller.export_controller import ExportController, export_bp
from web.middleware.jwt_auth import create_jwt_middleware

def create_app() -> Flask:
    app = Flask(__name__, 
                static_folder='static', 
                template_folder='templates')
    
    # Настройка логирования
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    logger = logging.getLogger(__name__)
    
    # Инициализация DI контейнера
    container = Container()
    
    # =========================================================================
    # Регистрация контроллеров и маршрутов
    # =========================================================================
    
    # 1. Модуль аутентификации
    auth_controller = AuthController(
        authenticate_use_case=container.authenticate_user_use_case(),
        jwt_provider=container.jwt_provider
    )
    auth_controller.register_routes()
    
    # 2. Модуль управления конфигурацией
    config_controller = ConfigController(
        get_config_use_case=container.get_configuration_use_case(),
        update_config_use_case=container.update_configuration_use_case()
    )
    config_controller.register_routes()
    
    # 3. Модуль управления жизненным циклом сервиса
    service_controller = ServiceController(
        get_status_use_case=container.get_service_status_use_case(),
        restart_use_case=container.restart_service_use_case()
    )
    service_controller.register_routes()

    # 4. Модуль обмена данными (Exchange)
    export_controller = ExportController(
        generate_manifest_uc=container.generate_export_manifest_use_case(),
        serve_file_uc=container.serve_file_by_token_use_case(),
        process_ack_uc=container.process_acknowledgment_use_case(),
        export_config_repo=container.export_config_repository
    )
    export_controller.register_routes()
    
    # =========================================================================
    # Регистрация Blueprints и Middleware
    # =========================================================================
    app.register_blueprint(auth_bp)
    app.register_blueprint(config_bp)
    app.register_blueprint(service_bp)
    app.register_blueprint(export_bp)
    
    # Глобальный middleware для проверки JWT
    app.before_request(create_jwt_middleware(container.jwt_provider))
    
    # =========================================================================
    # Фоновые задачи (Push-модель)
    # =========================================================================
    scheduler = container.background_scheduler()
    scheduler.start()
    logger.info("Background Push-dispatcher started.")
    
    # Регистрация хука для корректной остановки планировщика при завершении процесса
    atexit.register(lambda: scheduler.stop())

    # =========================================================================
    # Маршруты и обработчики ошибок
    # =========================================================================
    
    @app.route('/')
    def index():
        return render_template('index.html')

    @app.errorhandler(Exception)
    def handle_exception(e):
        logger.exception("Unhandled exception occurred")
        return jsonify({"error": "Internal server error"}), 500

    @app.errorhandler(404)
    def handle_not_found(e):
        return jsonify({"error": "Resource not found"}), 404

    @app.errorhandler(405)
    def handle_method_not_allowed(e):
        return jsonify({"error": "Method not allowed"}), 405

    return app

if __name__ == '__main__':
    app = create_app()
    app.run(host='0.0.0.0', port=5000, debug=True)