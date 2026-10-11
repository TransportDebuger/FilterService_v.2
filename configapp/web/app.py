import logging
from flask import Flask, jsonify
from di.container import Container
from web.controller.auth_controller import AuthController, auth_bp
from web.controller.config_controller import ConfigController, config_bp
from web.controller.service_controller import ServiceController, service_bp
from web.middleware.jwt_auth import create_jwt_middleware

app = Flask(__name__, 
            static_folder='static', 
            template_folder='templates')

from flask import render_template

@app.route('/')
def index():
    return render_template('index.html')

def create_app() -> Flask:
    app = Flask(__name__)
    
    # Настройка логирования
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger(__name__)
    
    # Инициализация DI контейнера
    container = Container()
    
    # Инициализация контроллеров и регистрация маршрутов
    auth_controller = AuthController(
        authenticate_use_case=container.authenticate_user_use_case(),
        jwt_provider=container._jwt_provider
    )
    auth_controller.register_routes()
    
    config_controller = ConfigController(
        get_config_use_case=container.get_configuration_use_case(),
        update_config_use_case=container.update_configuration_use_case()
    )
    config_controller.register_routes()
    
    service_controller = ServiceController(
        get_status_use_case=container.get_service_status_use_case(),
        restart_use_case=container.restart_service_use_case()
    )
    service_controller.register_routes()
    
    # Регистрация Blueprints
    app.register_blueprint(auth_bp)
    app.register_blueprint(config_bp)
    app.register_blueprint(service_bp)
    
    # Глобальный middleware для проверки JWT
    app.before_request(create_jwt_middleware(container._jwt_provider))
    
    # Глобальные обработчики ошибок
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