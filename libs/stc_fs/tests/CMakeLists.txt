# tests/CMakeLists.txt

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(stc_fs_test
    directory_monitor_test.cpp
    inotify_monitor_test.cpp
    polling_monitor_test.cpp
    mock_system_calls.hpp
)

# Указываем пути к внутренним заголовкам (src/) и к самим тестам
target_include_directories(stc_fs_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(stc_fs_test PRIVATE 
    stc::fs 
    GTest::gtest_main 
    GTest::gmock_main
)

add_test(NAME stc_fs_test COMMAND stc_fs_test)