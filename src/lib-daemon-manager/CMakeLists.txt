cmake_minimum_required(VERSION 3.15)
project(DaemonManager LANGUAGES CXX)

add_library(stc-daemon STATIC
    src/DaemonManager.cpp
)

target_include_directories(stc-daemon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(stc-daemon PUBLIC cxx_std_17)

# Для тестов
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()