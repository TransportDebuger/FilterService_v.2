# Инструкция по сборке и тестированию проекта

## Сборка для продуктивной среды

### Базовые команды

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

### Ключевые параметры:
| Флаг CMake              | Значение по умолчанию | Описание                                                                 |
|-------------------------|-----------------------|-------------------------------------------------------------------------|
| `-DBUILD_SHARED_LIBS=ON`| OFF                   | Сборка динамической библиотеки вместо статической                       |
| `-DCMAKE_INSTALL_PREFIX`| /opt/stc              | Путь для установки бинарных файлов                                      |
| `-DENABLE_DOXYGEN=ON`   | OFF                   | Генерация документации (требуется Doxygen)                             |

### Пример для продакшена:

cmake ..
-DCMAKE_BUILD_TYPE=Release
-DBUILD_SHARED_LIBS=ON
-DCMAKE_INSTALL_PREFIX=/usr/local

## Тестирование и проверки

### Полный цикл CI/CD

mkdir build && cd build
cmake .. -DENABLE_CODE_STYLE_CHECK=ON -DENABLE_VALGRIND_TESTS=ON
make check

### Основные флаги тестирования:
| Флаг CMake                  | По умолчанию | Описание                                                                 |
|-----------------------------|--------------|-------------------------------------------------------------------------|
| `-DENABLE_CODE_STYLE_CHECK` | ON           | Проверка стиля кода (clang-format + cppcheck)                          |
| `-DENABLE_UNIT_TESTS`       | ON           | Включение модульных тестов                                             |
| `-DENABLE_VALGRIND_TESTS`   | ON           | Проверка утечек памяти через Valgrind                                  |
| `-DENABLE_INTEGRATION_TESTS`| ON           | Запуск интеграционных тестов                                           |

### Запуск отдельных проверок:
1. **Проверка стиля кода:**

make style_check

2. **Модульные тесты:**

ctest -R DaemonManagerTest --output-on-failure

3. **Проверка памяти:**

ctest -R MemoryCheck --verbose

4. **Интеграционные тесты:**

ctest -R IntegrationTest --verbose

## Дополнительные опции

### Генерация документации

cmake .. -DENABLE_DOXYGEN=ON
make doc

Документация будет доступна в build/docs/html/index.html

### Установка зависимостей (Ubuntu):

sudo apt-get install -y
clang-format
cppcheck
valgrind
doxygen
graphviz

### Пример полной сборки с тестами:

mkdir build && cd build
cmake ..
-DENABLE_CODE_STYLE_CHECK=ON
-DENABLE_VALGRIND_TESTS=ON
-DENABLE_INTEGRATION_TESTS=ON
make -j$(nproc)
ctest --output-on-failure

## Особенности работы с разными конфигурациями

### Release vs Debug
| Параметр          | Release                        | Debug                          |
|-------------------|--------------------------------|--------------------------------|
| Оптимизация       | -O3                           | -O0 -g                        |
| Тесты             | Отключены                     | Включены                      |
| Проверки стиля    | Отключены                     | Включены                      |
| Сантайм проверки  | Отключены (NDEBUG)            | Включены                      |

### Переопределение путей

cmake ..
-DCMAKE_INSTALL_PREFIX=/custom/path
-DLIBRARY_OUTPUT_PATH=/custom/lib
-DEXECUTABLE_OUTPUT_PATH=/custom/bin

## Устранение неполадок

### Типичные проблемы:
1. **Ошибки зависимостей:**
   - Автоматическая установка через CMake (только для Linux)
   - Ручная установка пакетов из раздела "Установка зависимостей"

2. **Ошибки стиля кода:**

clang-format -style=Google -i path/to/file.cpp

3. **Проблемы с PID-файлами:**

sudo rm /var/run/myservice.pid

4. **Ошибки Valgrind:**

valgrind --tool=memcheck --leak-check=full --track-origins=yes ./DaemonManagerTest


Для дополнительной информации см. `CMakeLists.txt` и документацию к используемым библиотекам.