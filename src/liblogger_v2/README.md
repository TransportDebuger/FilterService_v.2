## Сборка библиотеки

```
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=./install/path -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
cmake --install .
```

## Сборка тестов

```
cmake .. -DBUILD_TESTING=ON
```

## Использование в других проектах (CMake)

```
find_package(stclogger REQUIRED)
target_link_libraries(your_target PRIVATE stclogger)
```