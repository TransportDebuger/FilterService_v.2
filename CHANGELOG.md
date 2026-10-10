## [4.1.0] - 2026-09-28

### Added
- Добавлен класс `stc::MetricsHttpServer` для экспозиции метрик
  Prometheus по эндпоинту `/metrics` через минималистичный
  HTTP-сервер на базе POSIX-сокетов.
- Добавлено поле `metrics_port` в DTO `ApplicationConfiguration`
  и соответствующая валидация в `ConfigValidator`.
- Интегрирован запуск и остановка `MetricsHttpServer` в корень
  композиции `stc::ServiceController`.
- Добавлена расширенная документация Doxygen для модуля:
  `page_metrics_http_server.dox`, `class_metrics_http_server.dox`
  и файлы для нетривиальных методов (`fn_*.dox`).