/**
 * @file configmanager.hpp
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 * @brief Фасад для управления загрузкой, обработкой и кэшированием конфигурации
 * сервиса
 *
 * @details
 * Класс ConfigManager объединяет подсистемы загрузки (ConfigLoader), валидации
 * (ConfigValidator), подстановки переменных окружения (EnvironmentProcessor)
 * и кэширования (ConfigCache) JSON-конфигураций XML Filter Service.
 * Использует паттерн Singleton для единственного глобального доступа к
 * настройкам и паттерн Cache-Aside для ускорения повторного получения
 * окружений. Обеспечивает инициализацию, перезагрузку (хот-свап) и
 * предоставление “merged” конфигураций для разных окружений (development,
 * production и т.д.).
 *
 * @warning Не потокобезопасен при одновременном вызове методов изменения.
 */

#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

#include "../include/configcache.hpp"
#include "../include/configloader.hpp"
#include "../include/configvalidator.hpp"
#include "../include/enviromentprocessor.hpp"

/**
 * @defgroup Configuration Компоненты управления конфигурацией
 */

/**
 * @defgroup Getters Методы получения данных (getters)
 */

/**
 * @defgroup Initialization Методы инициализации структур данных
 */

/**
 * @defgroup Management Методы управления данными
 */

/**
 * @defgroup Constructors Конструкторы (методы создания структур данных)
 */

/**
 * @defgroup Destructors Деструкторы (методы уничтожения структур данных)
 */

/**
 * @class ConfigManager
 * @brief Класс управления жизненным циклом конфигурации
 *
 * @ingroup Configuration
 *
 * @details
 * Позволяет:
 *  - Инициализировать конфигурацию из JSON-файла и обработать её переменные
 * окружения
 *  - Перезагружать конфигурацию без перезапуска сервиса (hot reload)
 *  - Объединять секции “defaults” и “environments” в один JSON через
 * merge_patch
 *  - Кэшировать результаты слияния для быстрого доступа
 *
 * @note Singleton инициализируется лениво и потокобезопасно согласно C++11
 * @warning При перезагрузке сбрасывается весь кэш окружений
 */
class ConfigManager {
 public:

   friend class ConfigReloadTransaction;
  /**
     * @brief Возвращает единственный экземпляр ConfigManager
     * @ingroup Getters
     *
     * @details
     * Потокобезопасный метод инициализации синглтон-экземпляра.
     *
     * @return ConfigManager& Ссылка на глобальный экземпляр
     *
     * @note Местная статическая переменная инициализируется один раз
     * @code
     auto &mgr = ConfigManager::instance();
     @endcode
     */
  static ConfigManager &instance();

  /**
     * @brief Загружает конфигурацию из указанного JSON-файла
     * @ingroup Initialization
     *
     * @param[in] filename Путь к JSON-файлу конфигурации
     * @throw std::runtime_error В случае ошибок чтения, парсинга или валидации
     *
     * @details
     * 1. Читает файл через ConfigLoader::loadFromFile()
     * 2. Подставляет переменные среды через EnvironmentProcessor::process()
     * 3. Валидирует структуру через ConfigValidator::validateRoot()
     * 4. Очищает кэш окружений после успешной загрузки
     *
     * @warning Вызывает исключение при некорректном формате или недоступности
     файла
     * @code
     ConfigManager::instance().initialize("config.json");
     @endcode
     */
  void initialize(const std::string &filename);

  /**
     * @brief Перезагружает конфигурацию из последнего JSON-файла
     * @ingroup Management
     *
     * @throw std::runtime_error Если путь к файлу не задан или перезагрузка не
     удалась
     *
     * @details
     * 1. Сохраняет текущую конфигурацию в резерв
     * 2. Выполняет повторную загрузку и валидацию
     * 3. При сбое восстанавливает резерв, иначе сбрасывает кэш
     *
     * @note Используется для реактивного обновления по сигналу SIGHUP
     * @code
     ConfigManager::instance().reload();
     @endcode
     */
  void reload();

  /**
     * @brief Возвращает объединённую конфигурацию для заданного окружения
     * @ingroup Getters
     *
     * @param[in] env Имя окружения (например, "production")
     * @return nlohmann::json JSON-объект с merged-конфигурацией
     * @throw std::runtime_error Если окружение отсутствует в секции
     environments
     *
     * @details
     *  - Сначала проверяет кэш через ConfigCache::getCached()
     *  - Если нет, берёт "defaults" и применяет merge_patch с environments[env]
     *  - Сохраняет в кэше и возвращает результат
     *
     * @code
     auto cfg = ConfigManager::instance().getMergedConfig("production");
     @endcode
     */
  nlohmann::json getMergedConfig(const std::string &env) const;

  /**
     * @brief Возвращает путь к глобальному comparison_list из конфигурации
     * @ingroup Getters
     *
     * @param[in] env Имя окружения (in)
     * @return std::string Путь к CSV-файлу, заданный в config или
     "./comparison_list.csv" по умолчанию
     *
     * @details
     * Извлекает значение "comparison_list" из getMergedConfig(env).
     *
     * @code
     auto path = mgr.getGlobalComparisonList("development");
     @endcode
     */
  std::string getGlobalComparisonList(const std::string &env) const;

  /**
     * @brief Применяет CLI-переопределения к базовой конфигурации
     * @ingroup Management
     *
     * @param[in] overrides Пары ключ-значение для merge_patch (in)
     *
     * @details
     *  - Формирует JSON из overrides
     *  - Применяет merge_patch к baseConfig_
     *  - Обновляет кэш для всех окружений
     *
     * @warning Перезаписывает значения в baseConfig_ без обратного
     восстановления
     @code
     mgr.applyCliOverrides({{"defaults.log_level","debug"}});
     @endcode
     */
  void applyCliOverrides(
      const std::unordered_map<std::string, std::string> &overrides);

          /**
     * @brief Возвращает копию базовой конфигурации для резервирования
     * @return nlohmann::json Копия baseConfig_ без merge с окружениями
     */
    const nlohmann::json& getCurrentConfig() const;
    
    /**
     * @brief Восстанавливает базовую конфигурацию из резерва
     * @param backup Резервная копия для восстановления
     */
    void restoreFromBackup(const nlohmann::json& backup);

 private:
  /**
   * @brief Приватный конструктор Singleton
   * @ingroup Constructors
   */
  ConfigManager() = default;
  /**
   * @brief Приватный деструктор
   * @ingroup Destructors
   */
  ~ConfigManager() = default;

  /**
 * @brief Создает резервную копию текущей базовой конфигурации
 * @ingroup InternalMethods
 *
 * @details
 * Метод выполняет глубокое копирование содержимого `baseConfig_` в
 `backupConfig_`
 * для последующего восстановления в случае неудачной перезагрузки конфигурации.
 * Операция выполняется только если `baseConfig_` не пуст, что предотвращает
 * сохранение некорректного состояния.
 *
 * Алгоритм работы:
 * 1. Проверяет, что `baseConfig_` содержит валидные данные
 * 2. Выполняет полное копирование JSON-объекта в `backupConfig_`
 * 3. Сохраняет состояние для возможного отката операции
 *
 * @note Метод не выполняет валидацию содержимого конфигурации
 * @warning Требует предварительной инициализации `baseConfig_` через
 initialize()
 *
 * @code
 // Внутреннее использование перед reload()
 backupCurrentConfig();
 try {
     // Попытка загрузки новой конфигурации
 } catch (...) {
     restoreBackupConfig(); // Восстановление при ошибке
 }
 @endcode
 *
 * @see restoreBackupConfig(), reload()
 * @since 1.0
 */
  void backupCurrentConfig();

  /**
 * @brief Восстанавливает конфигурацию из резервной копии
 * @ingroup InternalMethods
 *
 * @details
 * Метод выполняет восстановление `baseConfig_` из ранее сохраненной копии
 * `backupConfig_` в случае неудачной перезагрузки конфигурации. Обеспечивает
 * откат к последнему валидному состоянию системы без потери работоспособности.
 *
 * Процесс восстановления:
 * 1. Проверяет наличие валидных данных в `backupConfig_`
 * 2. Копирует содержимое обратно в `baseConfig_`
 * 3. Выводит информационное сообщение о восстановлении
 * 4. Сохраняет работоспособность системы с предыдущей конфигурацией
 *
 * @note Вызывается автоматически при ошибках в методе reload()
 * @warning Не очищает кэш после восстановления - требует ручной очистки
 *
 * @code
 try {
     // Загрузка новой конфигурации
     baseConfig_ = newConfig;
 } catch (const std::exception& e) {
     restoreBackupConfig(); // Автоматический откат
     throw std::runtime_error("Config restore needed");
 }
 @endcode
 *
 * @see backupCurrentConfig(), reload()
 * @since 1.0
 */
  void restoreBackupConfig();

  /**
   * @brief Выполняет безопасную валидацию конфигурации без генерации исключений
   * @ingroup ValidationMethods
   *
   * @param[in] config JSON-объект конфигурации для проверки корректности
   * структуры
   * @return bool true если конфигурация прошла валидацию, false при обнаружении
   * ошибок
   *
   * @details
   * Метод служит обёрткой для `ConfigValidator::validateRoot()`,
   * перехватывающей все возможные исключения и возвращающей булевый результат
   * вместо их пробрасывания. Используется в критических участках кода, где
   * необходимо избежать аварийного завершения при некорректной конфигурации.
   *
   * Выполняемые проверки:
   * 1. Наличие обязательных секций "defaults" и "environments"
   * 2. Корректность типов данных в каждой секции
   * 3. Валидность структуры массивов sources и logging
   * 4. Соответствие полей источников их типам (local, ftp, sftp)
   *
   * @note Все ошибки валидации логируются в stderr для отладки
   * @warning Не модифицирует переданный config, выполняет только чтение
   *
   * @code
   * nlohmann::json testConfig = loadConfigFromFile("test.json");
   * if (validateConfigSafely(testConfig)) {
   *     baseConfig_ = std::move(testConfig);
   * } else {
   *     // Обработка некорректной конфигурации без исключений
   * }
   * @endcode
   *
   * @see ConfigValidator::validateRoot()
   * @since 1.0
   */
  bool validateConfigSafely(const nlohmann::json &config) const;

  /**
   * @brief Загрузчик конфигурационных файлов
   * @ingroup Components
   *
   * @details
   * Экземпляр класса ConfigLoader, отвечающий за чтение и парсинг JSON-файлов
   * конфигурации. Инкапсулирует логику работы с файловой системой и обработку
   * ошибок ввода-вывода. Поддерживает как начальную загрузку через
   * loadFromFile(), так и перезагрузку через reload().
   *
   * Основные возможности:
   * - Чтение JSON-файлов с валидацией синтаксиса
   * - Отслеживание пути к последнему загруженному файлу
   * - Детальная обработка ошибок парсинга и доступа к файлам
   * - Поддержка относительных и абсолютных путей к конфигурации
   *
   * @note Не является потокобезопасным, синхронизация обеспечивается
   * configMutex_
   * @warning Не выполняет кэширование загруженных файлов
   *
   * @see ConfigLoader, initialize(), reload()
   * @since 1.0
   */
  ConfigLoader loader_;

  /**
   * @brief Валидатор структуры и содержимого конфигурации
   * @ingroup Components
   *
   * @details
   * Экземпляр класса ConfigValidator для проверки корректности загруженных
   * JSON-конфигураций. Выполняет структурную валидацию, проверку типов данных
   * и соответствие полей требованиям спецификации XML Filter Service.
   *
   * Проверяемые элементы:
   * - Наличие обязательных корневых секций (defaults, environments)
   * - Корректность массивов sources с валидацией полей по типам источников
   * - Правильность конфигурации логгеров в секции logging
   * - Соответствие FTP/SFTP источников требованиям аутентификации
   *
   * @note Генерирует детальные сообщения об ошибках для упрощения отладки
   * @warning Не модифицирует проверяемые конфигурации, работает в режиме
   * read-only
   *
   * @see ConfigValidator, validateConfigSafely()
   * @since 1.0
   */
  ConfigValidator validator_;

  /**
   * @brief Процессор подстановки переменных окружения
   * @ingroup Components
   *
   * @details
   * Экземпляр класса EnvironmentProcessor для рекурсивной обработки шаблонов
   * вида `$ENV{VARIABLE}` во всех строковых узлах JSON-конфигурации.
   * Обеспечивает динамическую подстановку значений системных переменных
   * окружения в параметры конфигурации без необходимости жёсткого кодирования
   * путей и настроек.
   *
   * Обрабатываемые элементы:
   * - Строковые значения в объектах и массивах JSON
   * - Вложенные структуры любой глубины
   * - Множественные вхождения переменных в одной строке
   * - Шаблоны в именах файлов, путях и URL-адресах
   *
   * @note Использует стандартную функцию getenv() для получения значений
   * @warning Оставляет шаблоны неизменными при отсутствии переменной в
   * окружении
   *
   * @see EnvironmentProcessor, initialize(), reload()
   * @since 1.0
   */
  EnvironmentProcessor envProcessor_;

  /**
   * @brief Кэш обработанных конфигураций для различных окружений
   * @ingroup Components
   *
   * @details
   * Экземпляр класса ConfigCache для хранения результатов слияния секций
   * "defaults" и "environments" в памяти. Реализует паттерн Cache-Aside для
   * оптимизации повторных запросов конфигураций часто используемых окружений
   * и снижения вычислительной нагрузки на операции merge_patch.
   *
   * Кэшируемые данные:
   * - Объединённые конфигурации для production, development, testing
   * - Результаты применения CLI-переопределений
   * - Промежуточные состояния при горячей перезагрузке
   * - Конфигурации с подставленными переменными окружения
   *
   * @note Помечен как mutable для доступа из константных методов
   * getMergedConfig()
   * @warning Автоматически очищается при reload() и applyCliOverrides()
   *
   * @see ConfigCache, getMergedConfig(), clearCache()
   * @since 1.0
   */
  mutable ConfigCache cache_;

  /**
   * @brief Базовая конфигурация, загруженная из JSON-файла
   * @ingroup StateData
   *
   * @details
   * JSON-объект, содержащий полную конфигурацию, загруженную из файла после
   * обработки переменных окружения и валидации структуры. Служит источником
   * данных для формирования merged-конфигураций различных окружений и
   * применения CLI-переопределений. Модифицируется только при успешной загрузке
   * или reload.
   *
   * Структура содержимого:
   * - Секция "defaults" с базовыми настройками сервиса
   * - Секция "environments" со специфичными настройками окружений
   * - Глобальные параметры уровня приложения
   * - Результаты подстановки переменных окружения
   *
   * @note Используется как шаблон для создания environment-specific
   * конфигураций
   * @warning Прямое изменение может нарушить целостность кэшированных данных
   *
   * @see initialize(), reload(), getMergedConfig()
   * @since 1.0
   */
  nlohmann::json baseConfig_;

  /**
   * @brief Резервная копия конфигурации для операций отката
   * @ingroup StateData
   *
   * @details
   * JSON-объект, содержащий копию предыдущей валидной конфигурации для
   * восстановления в случае неудачной перезагрузки. Обеспечивает механизм
   * безопасного hot-reload без потери работоспособности сервиса при
   * некорректной новой конфигурации или ошибках файловой системы.
   *
   * Сценарии использования:
   * - Откат при синтаксических ошибках в новом JSON-файле
   * - Восстановление при сбое валидации структуры
   * - Возврат к стабильному состоянию при ошибках подстановки переменных
   * - Сохранение работоспособности при недоступности конфигурационного файла
   *
   * @note Обновляется автоматически перед каждой операцией reload()
   * @warning Не содержит кэшированные merged-конфигурации, только базовую
   * структуру
   *
   * @see backupCurrentConfig(), restoreBackupConfig(), reload()
   * @since 1.0
   */
  nlohmann::json backupConfig_;

  /**
   * @brief Путь к текущему конфигурационному файлу
   * @ingroup StateData
   *
   * @details
   * Строка, содержащая полный или относительный путь к JSON-файлу конфигурации,
   * используемому для операций перезагрузки. Устанавливается при вызове
   * initialize() и используется reload() для повторного чтения того же файла
   * без необходимости передачи параметров.
   *
   * Поддерживаемые форматы:
   * - Относительные пути: "./config.json", "configs/production.json"
   * - Абсолютные пути: "/etc/xmlfilter/config.json", "/opt/app/config.json"
   * - Пути с переменными окружения после их разрешения
   * - Символические ссылки (разрешаются автоматически ОС)
   *
   * @note Сохраняется в неизменном виде, как был передан в initialize()
   * @warning Не проверяется на актуальность при каждом обращении
   *
   * @see initialize(), reload(), ConfigLoader::getLastLoadedFile()
   * @since 1.0
   */
  std::string configFilePath_;

  /**
   * @brief Мьютекс для обеспечения потокобезопасности операций с конфигурацией
   * @ingroup Synchronization
   *
   * @details
   * Мьютекс для синхронизации доступа к внутреннему состоянию ConfigManager
   * при многопоточном использовании. Защищает критические секции кода,
   * включающие модификацию baseConfig_, работу с кэшем и операции чтения файлов
   * от гонок данных и состояния неопределённости.
   *
   * Защищаемые операции:
   * - Загрузка и перезагрузка конфигурации (initialize, reload)
   * - Доступ к кэшу и его модификация (getMergedConfig, clearCache)
   * - Применение CLI-переопределений (applyCliOverrides)
   * - Операции резервного копирования и восстановления
   *
   * @note Помечен как mutable для использования в константных методах чтения
   * @warning Используйте std::lock_guard для автоматического освобождения
   * блокировки
   *
   * @see initialize(), reload(), getMergedConfig(), applyCliOverrides()
   * @since 1.0
   */
  mutable std::mutex configMutex_; /** @brief Мьютекс для синхронизации */
};