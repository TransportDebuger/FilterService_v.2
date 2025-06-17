cmake_minimum_required(VERSION 3.15)
project(MetricsCollector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Опции конфигурации
option(BUILD_SHARED_LIBS "Сборка динамической библиотеки" OFF)
option(ENABLE_DOXYGEN "Генерация документации Doxygen" OFF)
option(ENABLE_UNIT_TESTS "Включение unit-тестов" ON)

# Пути установки
set(CMAKE_INSTALL_PREFIX "/opt/stc" CACHE PATH "Директория установки")
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Исходные файлы
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "include/*.hpp"
)

# Основная библиотека
if(BUILD_SHARED_LIBS)
    add_library(stc-metrics SHARED ${SOURCES})
else()
    add_library(stc-metrics STATIC ${SOURCES})
endif()

# Директории включения
target_include_directories(stc-metrics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Стандарт C++17
target_compile_features(stc-metrics PUBLIC cxx_std_17)

# Документация
if(ENABLE_DOXYGEN)
    find_package(Doxygen REQUIRED)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_PROJECT_NAME "MetricsCollector")
        set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
        doxygen_add_docs(docs
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            COMMENT "Генерация документации"
        )
    else()
        message(WARNING "Doxygen not found - documentation disabled")
    endif()
endif()

# Установка
install(TARGETS stc-metrics
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# Unit-тесты
# if(ENABLE_UNIT_TESTS)
#     enable_testing()
    
#     add_executable(metrics-test
#         tests/MetricsCollectorTests.cpp
#     )
    
#     target_link_libraries(metrics-test
#         PRIVATE stc-metrics
#         GTest::GTest 
#         GTest::Main 
#         pthread
#     )
    
#     add_test(NAME MetricsTest COMMAND metrics-test)
# endif()

# Группировка целей
# add_custom_target(check
#     COMMAND ctest --output-on-failure
#     COMMENT "Запуск unit-тестов"
# )