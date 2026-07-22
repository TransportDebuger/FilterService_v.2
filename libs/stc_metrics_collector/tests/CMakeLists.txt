# ==============================================================================
# Модуль тестирования stc::metrics (GoogleTest)
# ==============================================================================

include(GoogleTest)

# Для тестов многопоточности (std::jthread) требуется явная линковка с pthreads,
# так как сама библиотека stc::metrics не создает потоков и не линкуется с ними.
find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------
# 1. Тесты ядра библиотеки (Реестр, атомарные примитивы, NoOp, многопоточность)
# ------------------------------------------------------------------------------
add_executable(metrics_registry_test metrics_registry_test.cpp)

target_link_libraries(metrics_registry_test PRIVATE 
    stc::metrics 
    GTest::gtest_main
    Threads::Threads
)

# Автоматическое обнаружение тестов (TEST-макросов) для интеграции с CTest
gtest_discover_tests(metrics_registry_test)

# ------------------------------------------------------------------------------
# 2. Тесты сериализации (Prometheus Text Exposition Format)
# ------------------------------------------------------------------------------
add_executable(prometheus_exporter_test prometheus_exporter_test.cpp)

target_link_libraries(prometheus_exporter_test PRIVATE 
    stc::metrics 
    GTest::gtest_main
)

gtest_discover_tests(prometheus_exporter_test)