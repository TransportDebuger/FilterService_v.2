from enum import Enum

class ExchangeAction(str, Enum):
    """Действие, выполняемое с файлом после успешного подтверждения (ACK) или отправки."""
    DELETE = "delete"
    MOVE = "move"