cmake_minimum_required(VERSION 3.20)
project(stc_fs VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Глобальные настройки и стандарт C++20
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции для специализированных сборок
option(STC_FS_ENABLE_COVERAGE "Enable code coverage flags (--coverage)" OFF)
option(STC_FS_ENABLE_SANITIZERS "Enable Address and UndefinedBehavior Sanitizers" OFF)
option(STC_FS_BUILD_TESTS "Build unit tests for stc_fs" ON)

# ==============================================================================
# Определение цели библиотеки
# ==============================================================================
set(STC_FS_SOURCES
    src/directory_monitor.cpp
    src/inotify_monitor.cpp
    src/polling_monitor.cpp
    src/native_inotify_system_calls.cpp
)

add_library(stc_fs STATIC ${STC_FS_SOURCES})
add_library(stc::fs ALIAS stc_fs)

# ==============================================================================
# Применение флагов специализированных сборок
# ==============================================================================
if(STC_FS_ENABLE_COVERAGE)
    target_compile_options(stc_fs PRIVATE --coverage -O0 -g -fprofile-arcs -ftest-coverage)
    target_link_options(stc_fs PUBLIC --coverage)
endif()

if(STC_FS_ENABLE_SANITIZERS)
    target_compile_options(stc_fs PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -O0 -g)
    target_link_options(stc_fs PUBLIC -fsanitize=address,undefined)
endif()

# ==============================================================================
# Директории включения и зависимости
# ==============================================================================
target_include_directories(stc_fs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# stc::fs использует только стандартную библиотеку C++20 и системные вызовы POSIX.
# Явная линковка с pthread не требуется при использовании C++20 std::thread/jthread.

# ==============================================================================
# Конфигурация тестирования (Google Test)
# ==============================================================================
if(STC_FS_BUILD_TESTS)
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
install(TARGETS stc_fs
    EXPORT stc_fs_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/stc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT stc_fs_targets
    FILE stc_fs_targets.cmake
    NAMESPACE stc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stc_fs
)

# ==============================================================================
# Сбор списка исходных файлов для линтеров
# ==============================================================================
file(GLOB_RECURSE STC_FS_LINT_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/stc/fs/*.hpp
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
        COMMAND ${CLANG_FORMAT_BIN} --style=Google --dry-run --Werror ${STC_FS_LINT_FILES}
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
        ${STC_FS_LINT_FILES}
        COMMENT "Running cppcheck static analysis..."
    )
endif()

add_custom_target(linter-test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 1: clang-format ==="
    COMMAND ${CLANG_FORMAT_BIN} --style=Google --dry-run --Werror ${STC_FS_LINT_FILES}
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 2: cppcheck ==="
    COMMAND ${CPPCHECK_BIN} --enable=warning,style,performance,portability --std=c++20 --error-exitcode=1 --inline-suppr -i${CMAKE_CURRENT_BINARY_DIR} ${STC_FS_LINT_FILES}
    COMMENT "=== Running linter checks sequentially ==="
)

# ==============================================================================
# 2. UNIT-TEST (Сборка с покрытием и запуск)
# ==============================================================================
add_custom_target(unit-test
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_build
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/coverage_build
        -DSTC_FS_ENABLE_COVERAGE=ON -DSTC_FS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/coverage_build --target stc_fs_test
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_CURRENT_BINARY_DIR}/coverage_build --output-on-failure
    COMMENT "=== Building with coverage and running Unit Tests ==="
)

# ==============================================================================
# 3. MEM-TEST (Sanitizers + Valgrind)
# ==============================================================================
find_program(VALGRIND_BIN valgrind)

add_custom_target(mem-test
    # --- Этап 1: Sanitizers ---
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 1: Running Sanitizers [ASan/UBSan] ==="
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build
        -DSTC_FS_ENABLE_SANITIZERS=ON -DSTC_FS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build --target stc_fs_test
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_build --output-on-failure
    
    # --- Этап 2: Valgrind ---
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 2: Running Valgrind ==="
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build
        -DSTC_FS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build --target stc_fs_test
    COMMAND ${VALGRIND_BIN} --leak-check=full --show-leak-kinds=definite,indirect,possible --error-exitcode=1
        ${CMAKE_CURRENT_BINARY_DIR}/valgrind_build/tests/stc_fs_test
    COMMENT "=== Running Memory Tests [Sanitizers + Valgrind] ==="
)

# ==============================================================================
# 4. GCOV-REPORT (Генерация отчета о покрытии)
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
            '/usr/*' '*/third_party/*' '*/tests/*' '*/gtest/*' '*/include/*' '*/_deps/*'
            --output-file ${CMAKE_CURRENT_BINARY_DIR}/coverage.info.cleaned
        COMMAND ${GENHTML_BIN} ${CMAKE_CURRENT_BINARY_DIR}/coverage.info.cleaned
            --output-directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_report
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated at: ${CMAKE_CURRENT_BINARY_DIR}/coverage_report/index.html"
        COMMENT "=== Generating Coverage Report ==="
    )
endif()

# ==============================================================================
# 5. TEST-ALL (Последовательный запуск всех видов тестов)
# ==============================================================================
add_custom_target(test-all
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "=== RUNNING FULL TEST SUITE [ALL] ==="
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "[1/4] Linters..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target linter-test
    COMMAND ${CMAKE_COMMAND} -E echo "[2/4] Unit Tests and Coverage..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target unit-test
    COMMAND ${CMAKE_COMMAND} -E echo "[3/4] Memory Tests..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target mem-test
    COMMAND ${CMAKE_COMMAND} -E echo "[4/4] Coverage Report..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target gcov-report
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "=== ALL TESTS COMPLETED SUCCESSFULLY ==="
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
)

# ==============================================================================
# 6. DOCS (Генерация документации Doxygen)
# ==============================================================================
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYFILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
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