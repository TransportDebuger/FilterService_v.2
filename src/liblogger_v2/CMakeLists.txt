cmake_minimum_required(VERSION 3.12)
project(stclogger VERSION 1.0.0 LANGUAGES CXX)

# Опции
option(BUILD_SHARED_LIBS "Собрать динамическую библиотеку" OFF)
option(BUILD_TESTING "Собирать тесты" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Директории с исходными кодами
set(SOURCES
    src/ilogger.cpp
    src/consolelogger.cpp
)

# Публичные заголовочные файлы
set(PUBLIC_HEADERS
    include/stc/ilogger.hpp
    include/stc/consolelogger.hpp
)

# Создание библиотеки
add_library(stclogger ${SOURCES} ${PUBLIC_HEADERS})

# Установка публичных заголовочных файлов
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
    add_executable(stclogger_tests
        tests/consoleloggertests.cpp
    )
    
    # Линковка
    target_link_libraries(stclogger_tests
        PRIVATE
            stclogger
            GTest::GTest
            GTest::Main
    )
    
    # Добавление тестов в CTest
    add_test(NAME logger_tests COMMAND stclogger_tests)
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

# Генерация файла конфигурации
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/stclogger-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/stclogger-config.cmake
    INSTALL_DESTINATION /opt/lib/stclogger
)

install(
    EXPORT stclogger-targets
    FILE stclogger-targets.cmake
    DESTINATION /opt/lib/stclogger
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/stclogger-config.cmake
    DESTINATION lib/cmake/stclogger
)