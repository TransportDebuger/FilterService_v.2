cmake_minimum_required(VERSION 3.12)
project(stclogger VERSION 1.0.0 LANGUAGES CXX)

# Опции
option(BUILD_SHARED_LIBS "Собрать динамическую библиотеку" OFF)
option(BUILD_TESTING "Собирать тесты" ON)
option(ENABLE_COVERAGE "Включить покрытие кода (gcov)" OFF)

# Настройки компиляции
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
endif()

# Директории с исходными кодами
set(SOURCES
    src/ilogger.cpp
    src/consolelogger.cpp
    src/basefilelogger.cpp
    src/syncfilelogger.cpp
    src/asyncfilelogger.cpp
    src/compositelogger.cpp
)

# Публичные заголовочные файлы
set(PUBLIC_HEADERS
    include/stc/ilogger.hpp
    include/stc/consolelogger.hpp
    include/stc/basefilelogger.hpp
    include/stc/syncfilelogger.hpp
    include/stc/asyncfilelogger.hpp
    include/stc/irotatablelogger.hpp
    include/stc/compositelogger.hpp
)

# Создание библиотеки
add_library(stclogger ${SOURCES} ${PUBLIC_HEADERS})

# Пути для заголовков
target_include_directories(stclogger
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Настройки для Windows
if(WIN32)
    target_compile_definitions(stclogger PRIVATE _WIN32)
endif()

# Тестирование
if(BUILD_TESTING)
    include(CTest)
    enable_testing()
    
    find_package(GTest REQUIRED)
    
    # Исполняемый файл для тестов
    add_executable(tests
        tests/consolelogger_test.cpp
        tests/syncfilelogger_test.cpp
        tests/asyncfilelogger_test.cpp
        tests/timeformatter_test.cpp
    )
    
    # Линковка
    target_link_libraries(tests
        PRIVATE
            stclogger
            GTest::GTest 
            GTest::Main
    )
    
    # Добавление тестов в CTest
    add_test(NAME consolelogger_tests COMMAND tests)
    add_test(NAME syncfilelogger_tests COMMAND tests)
    add_test(NAME asyncfilelogger_tests COMMAND tests)
endif()

# Установка библиотеки и заголовков
install(
    TARGETS stclogger
    EXPORT stclogger-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(
    DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Генерация файла конфигурации для find_package()
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/stclogger-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/stclogger-config.cmake
    INSTALL_DESTINATION lib/cmake/stclogger
)

install(
    EXPORT stclogger-targets
    FILE stclogger-targets.cmake
    DESTINATION lib/cmake/stclogger
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/stclogger-config.cmake
    DESTINATION lib/cmake/stclogger
)