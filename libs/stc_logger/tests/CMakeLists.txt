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