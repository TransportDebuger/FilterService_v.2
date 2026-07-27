# tests/CMakeLists.txt

include(GoogleTest)

# ==============================================================================
# 1. Модульные тесты (Unit Tests)
# ==============================================================================
set(UNIT_TEST_SOURCES
    logger_test.cpp
    filters_test.cpp
    formatters_test.cpp
    rotation_policies_test.cpp
    sinks/console_sink_test.cpp
    sinks/file_sink_test.cpp
    sinks/async_file_sink_test.cpp
)

add_executable(stc_logger_unit_tests ${UNIT_TEST_SOURCES})

target_link_libraries(stc_logger_unit_tests
    PRIVATE
    stc_logger
    GTest::gtest
    GTest::gtest_main
    GTest::gmock        # Подключаем Google Mock для работы с MOCK_METHOD
    GTest::gmock_main   # Подключаем main-файл gmock
)

# Автоматическая регистрация тестов в CTest
gtest_discover_tests(stc_logger_unit_tests)

# ==============================================================================
# 2. Интеграционные тесты (Integration Tests)
# ==============================================================================
set(INTEGRATION_TEST_SOURCES
    integration/logger_integration_test.cpp
)

add_executable(stc_logger_integration_tests ${INTEGRATION_TEST_SOURCES})

target_link_libraries(stc_logger_integration_tests
    PRIVATE
    stc_logger
    GTest::gtest
    GTest::gtest_main
)

# Регистрация интеграционных тестов в CTest
gtest_discover_tests(stc_logger_integration_tests)

# ==============================================================================
# 3. Нагрузочные тесты и бенчмарки (Google Benchmark)
# ==============================================================================
include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)

# Принудительное отключение тестов, санитайзеров и установки самой библиотеки benchmark
# (Предотвращает конфликты с GoogleTest и засорение графа целей)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
# Отключаем санитайзеры внутри benchmark, если они включены глобально
set(BENCHMARK_ENABLE_SANITIZERS OFF CACHE BOOL "" FORCE) 

FetchContent_MakeAvailable(benchmark)

set(BENCHMARK_SOURCES
    benchmark_logger_test.cpp
)

add_executable(stc_logger_benchmarks ${BENCHMARK_SOURCES})
target_link_libraries(stc_logger_benchmarks
    PRIVATE
    stc_logger
    benchmark::benchmark
)