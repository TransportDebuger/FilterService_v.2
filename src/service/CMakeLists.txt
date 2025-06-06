cmake_minimum_required(VERSION 3.10)
project(XmlFilterService VERSION 1.0.0 LANGUAGES CXX)

# Настройки компиляции
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Директории с исходными кодами
set(SOURCES
    src/filterservice.cpp
    src/service_controller.cpp
    src/signal_handler.cpp
)

set(HEADER_FILES 
    include/service_controller.hpp
    include/signal_handler.hpp
)

find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED) # Для FTP

add_executable(xmlfilter
    src/main.cpp
    src/DaemonController.cpp
    src/ConfigManager.cpp
    src/FileQueue.cpp
    src/LocalFileSource.cpp
    src/FtpFileSource.cpp
    src/XmlProcessor.cpp
)

target_include_directories(xml_filter_daemon PRIVATE include)
target_link_libraries(xml_filter_daemon PRIVATE LibXml2::LibXml2 CURL::libcurl)