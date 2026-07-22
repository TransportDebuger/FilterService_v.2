# stc::metrics

Высокопроизводительная, потокобезопасная инфраструктурная библиотека для сбора и экспорта телеметрии в приложениях на C++20. 

Библиотека спроектирована с упором на отсутствие глобального состояния (синглтонов), строгое следование паттерну Dependency Injection (DI) и минимизацию накладных расходов на «горячем пути» (hot path) многопоточных приложений.

## Ключевые архитектурные особенности

* **Lock-Free примитивы:** Атомарные счетчики, датчики и гистограммы используют `std::memory_order_relaxed`, что предотвращает избыточные барьеры памяти и конфликты кэш-линий (cache-line bouncing).
* **Разделение фаз:** Строгое разделение однопоточной фазы регистрации (валидация, аллокации) и многопоточной фазы сбора (O(1) доступ к атомарным ячейкам без поиска по строкам).
* **Паттерн Visitor:** Интерфейсы примитивов изолированы от механизмов сериализации. Экспорт делегирован инфраструктурному слою (`PrometheusExporter`).
* **Null Object Pattern:** Предоставляется `NoOpMetricsRegistry` и набор заглушек, которые компилятор элиминирует в `nop`, гарантируя нулевые накладные расходы (Zero Overhead) при отключении метрик без использования `#ifdef`.
* **Кроссплатформенность:** В отличие от `stc::signals`, библиотека использует исключительно стандартную библиотеку C++20 и не имеет привязок к Linux-specific API.
* **Отсутствие внешних зависимостей:** Продакшен-код не требует сторонних библиотек. GoogleTest подключается исключительно для сборки тестов.

## Интеграция в основной проект

Для подключения рекомендуется использовать механизм `FetchContent`. 
**Важно:** Согласно принципу SSOT и во избежание конфликтов версий GoogleTest, а также засорения графа целей основного проекта, необходимо принудительно отключить сборку тестов и санитайзеров библиотеки до вызова `FetchContent_MakeAvailable`.

```cmake
include(FetchContent)

# Изоляция специализированных сборок stc::metrics
set(STC_METRICS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STC_METRICS_ENABLE_SANITIZERS OFF CACHE BOOL "" FORCE)
set(STC_METRICS_ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    stc_metrics
    GIT_REPOSITORY https://path/to/stc_metrics.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(stc_metrics)

# Линковка с бизнес-логикой
target_link_libraries(my_business_target PRIVATE stc::metrics)
```

## Быстрый старт

Регистрация метрик происходит в точке композиции (Composition Root), а дескрипторы передаются в бизнес-компоненты через конструкторы.

```cpp
#include <stc/metrics/metrics_registry.hpp>
#include <stc/metrics/prometheus_exporter.hpp>

// 1. Инициализация (однопоточный контекст)
auto registry = std::make_shared<stc::metrics::MetricsRegistry>();
auto counter = registry->RegisterCounter("requests_total", "Total processed requests");
auto histogram = registry->RegisterHistogram("duration_seconds", "Request latency", {0.1, 0.5, 1.0});

// 2. Сбор данных (многопоточный контекст, lock-free)
// Вызывается из пула рабочих потоков
counter->Increment(1.0); 
histogram->Observe(0.25);

// 3. Экспорт (Pull-модель, по запросу HTTP-сервера)
std::string payload = stc::metrics::ExportToPrometheus(*registry, "my_service");
```

## Система сборки и верификация

Библиотека предоставляет набор агрегированных CMake-целей для комплексной верификации кода. 

| Цель | Описание |
|------|----------|
| `linter-test` | Последовательный запуск `clang-format` (Google Style, `--Werror`) и `cppcheck`. Завершается ошибкой, если инструменты не найдены. |
| `unit-test` | Конфигурирует изолированную сборку с флагом `--coverage`, компилирует тесты и запускает `ctest`. |
| `mem-test` | Двухэтапная проверка: сборка с AddressSanitizer/UBSan, затем прогон тестов через `valgrind --leak-check=full`. |
| `gcov-report` | Генерация HTML-отчета о покрытии кода с помощью `lcov` и `genhtml`. |
| `test-all` | Последовательный запуск `linter-test`, `unit-test`, `mem-test` и `gcov-report`. |
| `docs` | Генерация API-документации через Doxygen. |

Для запуска полного цикла локальной верификации перед коммитом:
```bash
cmake -B build -S .
cmake --build build --target test-all
```

## Документация

Исчерпывающая техническая документация, включая архитектурные обоснования, сценарные примеры использования и диаграммы классов, генерируется с помощью Doxygen:

```bash
cmake --build build --target docs
```
Результат будет доступен в `build/docs/html/index.html`. 
Альтернативно, статические файлы расположены в директории `docs/` (согласно иерархии `DOC_STYLEGUIDE.md`).

