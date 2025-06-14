cmake_minimum_required(VERSION 3.15)
project(STCSignalRouter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Опции сборки и тестирования
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(ENABLE_DOXYGEN "Generate Doxygen documentation" OFF)
option(ENABLE_CODE_STYLE_CHECK "Enable code style checks (clang-format, cppcheck)" ON)
option(ENABLE_UNIT_TESTS "Enable unit tests" ON)
option(ENABLE_VALGRIND_TESTS "Enable memory checks (valgrind)" ON)
option(ENABLE_INTEGRATION_TESTS "Enable integration tests" ON)
option(INSTALL_DEV_FILES "Install development files" ON)

# Пути установки
set(CMAKE_INSTALL_PREFIX "/opt/stc" CACHE PATH "Installation directory")
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Источники
file(GLOB_RECURSE STC_HEADERS "include/*.hpp")
file(GLOB_RECURSE STC_SOURCES "src/*.cpp")

# Библиотека
if(BUILD_SHARED_LIBS)
    add_library(stc-signal-router SHARED ${STC_SOURCES} ${STC_HEADERS})
else()
    add_library(stc-signal-router STATIC ${STC_SOURCES} ${STC_HEADERS})
endif()

target_include_directories(stc-signal-router PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Doxygen-документация
if(ENABLE_DOXYGEN)
    find_package(Doxygen REQUIRED)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_PROJECT_NAME "STCSignalRouter")
        set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        add_custom_target(doc
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# Установка
install(TARGETS stc-signal-router
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
if(INSTALL_DEV_FILES)
    install(DIRECTORY include/ DESTINATION include)
endif()

# --------------------------- Тестирование и анализ ---------------------------

# Проверка стиля кода
if(ENABLE_CODE_STYLE_CHECK)
    find_program(CLANG_FORMAT_EXE clang-format)
    find_program(CPPCHECK_EXE cppcheck)
    if(NOT CLANG_FORMAT_EXE)
        message(STATUS "Installing clang-format...")
        execute_process(COMMAND sudo apt-get install -y clang-format)
    endif()
    if(NOT CPPCHECK_EXE)
        message(STATUS "Installing cppcheck...")
        execute_process(COMMAND sudo apt-get install -y cppcheck)
    endif()
    file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.hpp *.h)
    add_custom_target(style_check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror -style=Google ${ALL_SOURCE_FILES}
        COMMAND ${CPPCHECK_EXE} --enable=all --inconclusive --std=c++17 --error-exitcode=1 ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format and cppcheck"
    )
endif()

# Unit-тесты
# if(ENABLE_UNIT_TESTS)
#     enable_testing()
#     find_package(GTest REQUIRED)
#     add_executable(SignalRouterTest tests/SignalRouterUtest.cpp src/SignalRouter.cpp)
#     target_include_directories(SignalRouterTest PRIVATE include)
#     target_link_libraries(SignalRouterTest GTest::GTest GTest::Main pthread)
#     add_test(NAME SignalRouterTest COMMAND SignalRouterTest)
# endif()

# Valgrind memory check
# if(ENABLE_VALGRIND_TESTS)
#     find_program(VALGRIND_EXE valgrind)
#     if(NOT VALGRIND_EXE)
#         message(STATUS "Installing valgrind...")
#         execute_process(COMMAND sudo apt-get install -y valgrind)
#     endif()
#     add_test(NAME MemoryCheck
#         COMMAND ${VALGRIND_EXE} --leak-check=full --error-exitcode=1 $<TARGET_FILE:SignalRouterTest>
#     )
# endif()

# Интеграционные тесты
# if(ENABLE_INTEGRATION_TESTS)
#     find_program(BASH_EXE bash)
#     if(NOT BASH_EXE)
#         message(STATUS "Installing bash shell...")
#         execute_process(COMMAND sudo apt-get install -y bash)
#     endif()
#     add_test(NAME IntegrationTest
#         COMMAND ${BASH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_test.sh
#     )
# endif()

# Сборка перед тестами
if(ENABLE_UNIT_TESTS OR ENABLE_VALGRIND_TESTS OR ENABLE_INTEGRATION_TESTS)
    add_custom_target(build_tests
        DEPENDS SignalRouterTest
    )
    add_dependencies(SignalRouterTest stc-signal-router)
endif()

# Группировка для полного цикла CI
add_custom_target(check
    DEPENDS style_check build_tests
    COMMAND ctest --output-on-failure
    COMMENT "Running all checks and tests"
)

# Release-сборка: только библиотека и установка, без тестов
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ENABLE_UNIT_TESTS OFF CACHE BOOL "Disable unit tests in Release" FORCE)
    set(ENABLE_VALGRIND_TESTS OFF CACHE BOOL "Disable valgrind in Release" FORCE)
    set(ENABLE_INTEGRATION_TESTS OFF CACHE BOOL "Disable integration tests in Release" FORCE)
    set(ENABLE_CODE_STYLE_CHECK OFF CACHE BOOL "Disable code style check in Release" FORCE)
endif()