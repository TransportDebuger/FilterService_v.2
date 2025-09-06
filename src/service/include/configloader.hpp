/**
 * @file configloader.hpp
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 * @brief Загрузчик конфигураций из JSON-файлов
 *
 * @details
 * Модуль предоставляет функциональность для загрузки и перезагрузки
 * конфигураций из JSON-файлов. Реализует паттерн "File Reader" с поддержкой
 * кэширования последнего загруженного файла для операций перезагрузки.
 * Обеспечивает надежное чтение конфигурационных файлов с детальной обработкой
 * ошибок.
 *
 * Ключевые особенности реализации:
 * - Поддержка полного парсинга JSON с валидацией синтаксиса
 * - Кэширование пути к последнему загруженному файлу
 * - Детальная обработка ошибок ввода-вывода и парсинга
 * - Интеграция с библиотекой nlohmann/json для оптимальной производительности
 *
 * Используемые паттерны:
 * - File Reader Pattern для чтения конфигурационных файлов
 * - Error Handling Pattern для обработки исключительных ситуаций
 * - State Pattern для отслеживания последнего загруженного файла
 *
 * @version 1.0
 * @since 1.0
 * @see ConfigManager, ConfigCache
 */

#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

/**
 * @defgroup Configuration Компоненты управления конфигурацией
 */

/**
 * @defgroup Constructors Конструкторы (методы создания структур данных)
 */

/**
 * @defgroup Destructors Деструкторы (методы уничтожения структур данных)
 */

/**
 * @defgroup MainAPI Основные методы
 */

/**
 * @defgroup Getters Методы получения данных (getters)
 */

/**
 * @defgroup ValidationMethods Методы валидации данных
 */

/**
 * @defgroup InternalMethods Внутренние (приватные) методы классов
 */

/**
 * @class ConfigLoader
 * @brief Загрузчик конфигураций из JSON-файлов
 * @ingroup Configuration
 *
 * @details
 * Класс предоставляет функциональность для загрузки конфигураций из JSON-файлов
 * с поддержкой операций перезагрузки. Обеспечивает надежное чтение файлов
 * с детальной обработкой ошибок парсинга и ввода-вывода. Используется как
 * базовый компонент системы управления конфигурациями.
 *
 * Основные возможности:
 * - Загрузка конфигурации из указанного JSON-файла
 * - Перезагрузка ранее загруженной конфигурации
 * - Валидация синтаксиса JSON с подробными сообщениями об ошибках
 * - Отслеживание пути к последнему загруженному файлу
 *
 * @note Класс не является потокобезопасным. Для многопоточного использования
 *       требуется внешняя синхронизация
 * @warning Требует наличия библиотеки nlohmann/json для корректной работы
 *
 * @see ConfigManager
 * @since 1.0
 */
class ConfigLoader {
 public:
  /**
     * @brief Конструктор по умолчанию
     * @ingroup Constructors
     *
     * @details
     * Инициализирует экземпляр ConfigLoader с пустым состоянием.
     * После создания объекта необходимо вызвать loadFromFile() для
     * загрузки первой конфигурации.
     *
     * @note Не выполняет никаких операций ввода-вывода
     * @warning Вызов reload() до loadFromFile() приведет к исключению
     *
     * @code
     ConfigLoader loader;
     // Объект готов к использованию
     @endcode
     */
  ConfigLoader() = default;

  /**
   * @brief Деструктор
   * @ingroup Destructors
   *
   * @details
   * Освобождает ресурсы, используемые объектом ConfigLoader.
   * Автоматически закрывает открытые файловые дескрипторы, если таковые
   * имеются.
   *
   * @note Не генерирует исключений
   */
  ~ConfigLoader() = default;

  /**
     * @brief Загружает конфигурацию из указанного JSON-файла
     * @ingroup MainAPI
     *
     * @details
     * Основной метод класса для загрузки конфигурации из JSON-файла.
     * Выполняет полный цикл: открытие файла, чтение содержимого, парсинг JSON
     * и валидацию синтаксиса. Сохраняет путь к файлу для последующих операций
     * перезагрузки. Поддерживает как относительные, так и абсолютные пути.
     *
     * Процесс загрузки включает:
     * 1. Валидацию входного параметра filename
     * 2. Открытие файла для чтения
     * 3. Парсинг JSON-содержимого
     * 4. Сохранение пути файла для reload()
     *
     * @param[in] filename Путь к JSON-файлу конфигурации (относительный или
     абсолютный)
     * @return nlohmann::json Объект с распарсенной конфигурацией
     *
     * @throw std::invalid_argument Если filename пустой или содержит
     недопустимые символы
     * @throw std::runtime_error При ошибках открытия файла или недостатке прав
     доступа
     * @throw nlohmann::json::parse_error При синтаксических ошибках JSON
     *
     * @note Метод сохраняет путь к файлу для последующих вызовов reload()
     * @warning Файл должен быть доступен для чтения и содержать валидный JSON
     *
     * @code
     ConfigLoader loader;
     try {
         auto config = loader.loadFromFile("config.json");
         // Использование загруженной конфигурации
         std::string dbHost = config["database"]["host"];
     } catch (const std::exception& e) {
         // Обработка ошибок загрузки
         std::cerr << "Config load error: " << e.what() << std::endl;
     }
     @endcode
     *
     * @see reload(), readFileContents()
     */
  nlohmann::json loadFromFile(const std::string &filename);

  /**
     * @brief Перезагружает конфигурацию из указанного файла
     * @ingroup MainAPI
     *
     * @details
     * Метод выполняет перезагрузку конфигурации из указанного файла.
     * Используется для обновления конфигурации во время работы приложения
     * без перезапуска. Выполняет те же операции валидации и парсинга,
     * что и loadFromFile(), но работает с переданным путем к файлу.
     *
     * Метод полезен в следующих сценариях:
     * - Обновление конфигурации по сигналу SIGHUP
     * - Переключение между различными конфигурационными файлами
     * - Откат к предыдущей версии конфигурации
     *
     * @param[in] currentFile Путь к файлу конфигурации для перезагрузки
     * @return nlohmann::json Обновленный объект конфигурации
     *
     * @throw std::runtime_error Если указанный файл недоступен или поврежден
     * @throw std::invalid_argument Если currentFile пустой
     * @throw nlohmann::json::parse_error При синтаксических ошибках JSON
     *
     * @note Метод не изменяет внутреннее состояние объекта (lastLoadedFile)
     * @warning Убедитесь в существовании и доступности файла перед вызовом
     *
     * @code
     ConfigLoader loader;
     loader.loadFromFile("config.json");

     // Перезагрузка при получении сигнала
     try {
         auto updatedConfig = loader.reload("config.json");
         // Применение обновленной конфигурации
     } catch (const std::exception& e) {
         // Обработка ошибок перезагрузки
     }
     @endcode
     *
     * @see loadFromFile(), readFileContents()
     */
  nlohmann::json reload(const std::string &currentFile);

  /**
     * @brief Возвращает путь к последнему загруженному файлу
     * @ingroup Getters
     *
     * @details
     * Метод предоставляет доступ к пути последнего успешно загруженного
     * конфигурационного файла. Используется для отладки, логирования
     * и информационных целей.
     *
     * @return std::string Путь к последнему загруженному файлу или пустая
     строка
     *
     * @note Возвращает пустую строку, если loadFromFile() не был вызван
     * @warning Путь может стать недействительным при перемещении файлов
     *
     * @code
     ConfigLoader loader;
     loader.loadFromFile("config.json");
     std::cout << "Loaded from: " << loader.getLastLoadedFile() << std::endl;
     @endcode
     */
  std::string getLastLoadedFile() const;

  /**
   * @brief Проверяет, был ли загружен какой-либо файл
   * @ingroup ValidationMethods
   *
   * @details
   * Вспомогательный метод для проверки состояния объекта ConfigLoader.
   * Возвращает true, если был успешно загружен хотя бы один конфигурационный
   * файл с помощью loadFromFile().
   *
   * @return bool true, если файл был загружен; false в противном случае
   *
   * @note Метод не проверяет текущую доступность файла
   * @warning Не гарантирует актуальность загруженной конфигурации
   *
   * @code
   // Пример использования
   ConfigLoader loader;
   if (!loader.hasLoadedFile()) {
       auto config = loader.loadFromFile("default.json");
   }
   @endcode
   */
  bool hasLoadedFile() const;

 private:
  /**
     * @brief Внутренний метод чтения и парсинга содержимого файла
     * @ingroup InternalMethods
     *
     * @details
     * Низкоуровневый метод для чтения содержимого JSON-файла и его парсинга.
     * Выполняет операции открытия файла, чтения данных в память и парсинга
     * с помощью библиотеки nlohmann/json. Используется внутренними методами
     * loadFromFile() и reload() для унификации процесса чтения.
     *
     * Алгоритм работы:
     * 1. Открытие файла в режиме чтения
     * 2. Проверка успешности открытия
     * 3. Передача потока парсеру JSON
     * 4. Обработка ошибок парсинга
     *
     * @param[in] filename Путь к файлу для чтения
     * @return nlohmann::json Распарсенный JSON объект
     *
     * @throw std::runtime_error При ошибках открытия файла или ввода-вывода
     * @throw nlohmann::json::parse_error При синтаксических ошибках JSON
     * @throw std::bad_alloc При нехватке памяти для загрузки файла
     *
     * @note Использует стандартный парсер nlohmann/json без дополнительных
     опций
     * @warning Не обрабатывает кодировки, отличные от UTF-8
     * @warning Загружает весь файл в память, что может быть неэффективно для
     больших файлов
     *
     * @code
     // Внутреннее использование (пример для понимания логики):
     try {
         auto config = readFileContents("config.json");
         // config содержит распарсенный JSON
     } catch (const nlohmann::json::parse_error& e) {
         // Обработка ошибок JSON
     }
     @endcode
     */
  nlohmann::json readFileContents(const std::string &filename) const;

  /**
   * @brief Путь к последнему успешно загруженному файлу
   * @details Сохраняет полный путь к файлу, загруженному последним вызовом
   * loadFromFile(). Используется для операций перезагрузки и информационных
   * запросов. Пустая строка означает, что ни один файл не был загружен.
   */
  std::string lastLoadedFile;
};