from domain.ports.service_controller import ServiceController
from application.dto.service_dto import ServiceStatusResponseDTO

class GetServiceStatusUseCase:
    def __init__(self, service_controller: ServiceController):
        self._service_controller = service_controller

    def execute(self) -> ServiceStatusResponseDTO:
        state = self._service_controller.get_status()
        return ServiceStatusResponseDTO(
            is_active=state.is_active,
            pid=state.pid,
            status_text=state.status_text
        )