cmake_minimum_required(VERSION 3.20)
project(stc_logger VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Глобальные настройки и стандарт C++20
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции для специализированных сборок
option(STC_LOGGER_ENABLE_COVERAGE "Enable code coverage flags (--coverage)" OFF)
option(STC_LOGGER_ENABLE_SANITIZERS "Enable Address and UndefinedBehavior Sanitizers" OFF)
option(STC_LOGGER_BUILD_TESTS "Build unit and integration tests for stc_logger" ON)

# ==============================================================================
# Определение цели библиотеки (Явное перечисление всех исходных файлов)
# ==============================================================================
set(STC_LOGGER_SOURCES
    src/logger.cpp
    src/filters/composite_filter.cpp
    src/filters/level_filter.cpp
    src/filters/source_filter.cpp
    src/formatters/json_formatter.cpp
    src/formatters/text_formatter.cpp
    src/formatters/xml_formatter.cpp
    src/sinks/async_file_sink.cpp
    src/sinks/circular_rotation_policy.cpp
    src/sinks/console_sink.cpp
    src/sinks/file_sink.cpp
    src/sinks/size_rotation_policy.cpp
    src/sinks/time_rotation_policy.cpp
)

add_library(stc_logger ${STC_LOGGER_SOURCES})
add_library(stc::logger ALIAS stc_logger)

# ==============================================================================
# Применение флагов специализированных сборок (СТРОГО ПОСЛЕ создания цели)
# ==============================================================================
if(STC_LOGGER_ENABLE_COVERAGE)
    target_compile_options(stc_logger PRIVATE --coverage -O0 -g -fprofile-arcs -ftest-coverage)
    target_link_options(stc_logger PUBLIC --coverage)
endif()

if(STC_LOGGER_ENABLE_SANITIZERS)
    target_compile_options(stc_logger PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -O0 -g)
    target_link_options(stc_logger PUBLIC -fsanitize=address,undefined)
endif()

# ==============================================================================
# Директории включения и зависимости
# ==============================================================================
target_include_directories(stc_logger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(stc_logger PUBLIC Threads::Threads)

# ==============================================================================
# Конфигурация тестирования (Google Test)
# ==============================================================================
if(STC_LOGGER_BUILD_TESTS)
    enable_testing()
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

# ==============================================================================
# Правила установки (для переиспользования в других проектах)
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS stc_logger
    EXPORT stc_logger_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/stc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT stc_logger_targets
    FILE stc_logger_targets.cmake
    NAMESPACE stc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stc_logger
)

# ==============================================================================
# Сбор списка исходных файлов для линтеров
# ==============================================================================
file(GLOB_RECURSE STC_LOGGER_LINT_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

# ==============================================================================
# 1. LINTER-TEST (Проверка стиля и статический анализ)
# ==============================================================================
find_program(CLANG_FORMAT_BIN clang-format)
find_program(CPPCHECK_BIN cppcheck)

if(NOT CLANG_FORMAT_BIN)
    add_custom_target(format-check COMMAND ${CMAKE_COMMAND} -E false COMMENT "FATAL: clang-format not found")
else()
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --style=Google --dry-run --Werror ${STC_LOGGER_LINT_FILES}
        COMMENT "Running clang-format (Google Style)..."
    )
endif()

if(NOT CPPCHECK_BIN)
    add_custom_target(cppcheck-test COMMAND ${CMAKE_COMMAND} -E false COMMENT "FATAL: cppcheck not found")
else()
    add_custom_target(cppcheck-test
        COMMAND ${CPPCHECK_BIN} --enable=warning,style,performance,portability 
                --std=c++20 --error-exitcode=1 --inline-suppr 
                -i${CMAKE_CURRENT_BINARY_DIR} 
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include
        COMMENT "Running cppcheck static analysis..."
    )
endif()

add_custom_target(linter-test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 1: clang-format ==="
    COMMAND ${CLANG_FORMAT_BIN} --style=Google --dry-run --Werror ${STC_LOGGER_LINT_FILES}
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 2: cppcheck ==="
    COMMAND ${CPPCHECK_BIN} --enable=warning,style,performance,portability --std=c++20 --error-exitcode=1 --inline-suppr -i${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include
    COMMENT "=== Running linter checks sequentially ==="
)

# ==============================================================================
# 2. UNIT-TEST (Сборка с покрытием и запуск)
# ==============================================================================
add_custom_target(unit-test
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_build
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/coverage_build 
            -DSTC_LOGGER_ENABLE_COVERAGE=ON -DSTC_LOGGER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/coverage_build --target stc_logger_unit_tests stc_logger_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_CURRENT_BINARY_DIR}/coverage_build --output-on-failure
    COMMENT "=== Building with coverage and running Unit Tests ==="
)

# ==============================================================================
# 3. MEM-TEST (Sanitizers + Valgrind)
# ==============================================================================
add_custom_target(mem-test
    # --- Этап 1: Sanitizers ---
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 1: Running Sanitizers [ASan/UBSan] ==="
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build 
            -DSTC_LOGGER_ENABLE_SANITIZERS=ON -DSTC_LOGGER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    # ИСПРАВЛЕНО: Добавлена сборка интеграционных тестов
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build --target stc_logger_unit_tests stc_logger_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build --output-on-failure
    
    # --- Этап 2: Valgrind ---
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 2: Running Valgrind ==="
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build 
            -DSTC_LOGGER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    # ИСПРАВЛЕНО: Добавлена сборка интеграционных тестов
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build --target stc_logger_unit_tests stc_logger_integration_tests
    # Примечание: Valgrind запускается только для unit-тестов, чтобы избежать 
    # чрезмерного замедления из-за интеграционных тестов с реальным I/O.
    COMMAND valgrind --leak-check=full --show-leak-kinds=definite,indirect,possible --error-exitcode=1
            ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build/tests/stc_logger_unit_tests
            
    COMMENT "=== Running Memory Tests [Sanitizers + Valgrind] ==="
)

# ==============================================================================
# 4. INTEGR-TEST (Интеграционные тесты)
# ==============================================================================
add_custom_target(integr-test
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "Integration"
    DEPENDS stc_logger_integration_tests
    COMMENT "=== Running Integration Tests ==="
)

# ==============================================================================
# 5. GCOV-REPORT (Генерация отчета о покрытии)
# ==============================================================================
find_program(LCOV_BIN lcov)
find_program(GENHTML_BIN genhtml)

if(NOT LCOV_BIN OR NOT GENHTML_BIN)
    add_custom_target(gcov-report COMMAND ${CMAKE_COMMAND} -E false COMMENT "FATAL: lcov or genhtml not found.")
else()
    add_custom_target(gcov-report
        COMMAND ${LCOV_BIN} --directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_build --zerocounters
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target unit-test
        COMMAND ${LCOV_BIN} --capture --directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_build 
                --output-file ${CMAKE_CURRENT_BINARY_DIR}/coverage.info
        COMMAND ${LCOV_BIN} --remove ${CMAKE_CURRENT_BINARY_DIR}/coverage.info
                '/usr/*' '*/third_party/*' '*/tests/*' '*/gtest/*' '*/include/*'
                --output-file ${CMAKE_CURRENT_BINARY_DIR}/coverage.info.cleaned
        COMMAND ${GENHTML_BIN} ${CMAKE_CURRENT_BINARY_DIR}/coverage.info.cleaned 
                --output-directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_report
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated at: ${CMAKE_CURRENT_BINARY_DIR}/coverage_report/index.html"
        COMMENT "=== Generating Coverage Report ==="
    )
endif()

# ==============================================================================
# 6. TEST-ALL (Последовательный запуск всех видов тестов)
# ==============================================================================
add_custom_target(test-all
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "=== RUNNING FULL TEST SUITE [ALL] ==="
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    
    COMMAND ${CMAKE_COMMAND} -E echo "[1/5] Linters..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target linter-test
    
    COMMAND ${CMAKE_COMMAND} -E echo "[2/5] Unit Tests and Coverage..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target unit-test
    
    COMMAND ${CMAKE_COMMAND} -E echo "[3/5] Memory Tests..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target mem-test
    
    COMMAND ${CMAKE_COMMAND} -E echo "[4/5] Integration Tests..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target integr-test
    
    COMMAND ${CMAKE_COMMAND} -E echo "[5/5] Coverage Report..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target gcov-report
    
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "=== ALL TESTS COMPLETED SUCCESSFULLY ==="
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
)

# ==============================================================================
# 7. DOCS (Генерация документации Doxygen)
# ==============================================================================
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Предполагается, что конфигурационный файл Doxyfile находится в директории docs/
    set(DOXYFILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_PATH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen..."
        VERBATIM
    )
else()
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "FATAL: Doxygen not found. Please install Doxygen to generate documentation."
    )
endif()