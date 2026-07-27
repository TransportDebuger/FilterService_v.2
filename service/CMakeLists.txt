cmake_minimum_required(VERSION 3.20)
project(XmlFilterService VERSION 2.1.0 LANGUAGES CXX)

# ==============================================================================
# 1. Настройки компиляции
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Включаем строгие предупреждения компилятора для раннего выявления ошибок
add_compile_options(-Wall -Wextra -Wpedantic)

# ==============================================================================
# 2. Внешние зависимости (Системные библиотеки)
# ==============================================================================
# Библиотеки для работы с XML и FTP
find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# nlohmann/json: Пытаемся найти в системе, если нет - подтягиваем через FetchContent
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    # Отключаем сборку тестов и примеров самой библиотеки json
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ==============================================================================
# 3. Инфраструктурные библиотеки (Монорепозиторий)
# ==============================================================================
# Принудительно отключаем тесты и санитайзеры для инфраструктурных библиотек,
# чтобы не засорять граф сборки основного сервиса и избежать конфликтов версий.
set(STC_FS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STC_LOGGER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STC_SIGNALS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STC_METRICS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Подключаем исходники библиотек из директории libs/
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/stc_fs ${CMAKE_BINARY_DIR}/stc_fs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/stc_logger ${CMAKE_BINARY_DIR}/stc_logger)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/stc_signal_router ${CMAKE_BINARY_DIR}/stc_signal_router)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/stc_metrics_collector ${CMAKE_BINARY_DIR}/stc_metrics_collector)

# ==============================================================================
# 4. Сборка исходных файлов сервиса
# ==============================================================================
# Используем GLOB_RECURSE для автоматического включения всех .cpp файлов 
# из новой многослойной структуры (application, domain, infrastructure).
# Это избавляет от необходимости вручную прописывать каждый перемещенный файл.
file(GLOB_RECURSE SERVICE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# ==============================================================================
# 5. Целевой исполняемый файл
# ==============================================================================
add_executable(xmlfilter ${SERVICE_SOURCES})

# ==============================================================================
# 6. Настройка путей и линковка
# ==============================================================================
# Указываем src/ как корневую директорию для include.
# Это позволяет использовать короткие пути в коде: 
# #include "sourceconfig.hpp" или #include "configmanager.hpp"
target_include_directories(xmlfilter PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Линкуем системные и инфраструктурные библиотеки.
# Примечание: Если в CMakeLists.txt библиотек не созданы алиасы (stc::fs), 
# замените их на реальные имена целей: stc_fs, stc_logger, stc_signal_router, stc_metrics_collector.
target_link_libraries(xmlfilter PRIVATE 
    # Системные зависимости
    LibXml2::LibXml2
    CURL::libcurl
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    
    # Инфраструктурные библиотеки проекта
    stc::fs
    stc::logger
    stc::signals
    stc::metrics
)

# ==============================================================================
# 7. Установка (Опционально, для сборки дистрибутива)
# ==============================================================================
install(TARGETS xmlfilter DESTINATION bin)