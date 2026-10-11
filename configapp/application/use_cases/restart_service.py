from domain.ports.service_controller import ServiceController

class RestartServiceUseCase:
    def __init__(self, service_controller: ServiceController):
        self._service_controller = service_controller

    def execute(self) -> None:
        self._service_controller.restart()