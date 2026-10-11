import jsonschema
from typing import Any, Dict

class ConfigValidationError(Exception):
    """Исключение, возникающее при несоответствии конфигурации JSON Schema."""
    pass

class ConfigValidator:
    def __init__(self, schema: Dict[str, Any]):
        self._schema = schema

    def validate(self, config: Dict[str, Any]) -> None:
        try:
            jsonschema.validate(instance=config, schema=self._schema)
        except jsonschema.ValidationError as e:
            raise ConfigValidationError(f"Configuration validation failed: {e.message}") from e
        except jsonschema.SchemaError as e:
            raise ConfigValidationError(f"Invalid JSON Schema: {e.message}") from e