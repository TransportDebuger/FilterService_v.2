/**
 * @file sourceconfig.hpp
 * @brief Описывает конфигурацию источника данных
 *
 * @details
 * Модуль определяет структуру `SourceConfig`, включающую параметры для
 * различных типов файловых хранилищ (локальные, SMB, FTP) с с поддержкой
 * параметров подключения, валидации и фильтрации XML.
 * Реализует многокритериальную фильтрацию, настраиваемую через
 * XPath-выражения и веса критериев. Использует
 * шаблон Builder при сериализации и десериализации JSON.
 *
 * Ключевые особенности реализации:
 * - Многокритериальная фильтрация
 * - Поддержка нескольких критериев фильтрации через массив criteria
 * - Каждый критерий содержит XPath-выражение, атрибут и столбец CSV для
 * сравнения
 * - Возможность указания обязательности критерия через флаг required
 *
 * Для фильтров поддерживаются логические операторы:
 * AND - все критерии должны совпадать
 * OR - достаточно одного совпадения
 * MAJORITY - большинство критериев должны совпадать
 * WEIGHTED - взвешенная сумма превышает порог
 *
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date June, 2025
 * @version 1.0
 */

#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Конфигурация контроля количества записей в документе
 */
struct RecordCountConfig {
    std::string xpath;        ///< XPath к элементу, например "//Export" или "/docroot/recordCount"
    std::string attribute;    ///< Имя атрибута или свойства, например "recordCount" или "value"
    bool enabled;             ///< Включена ли обработка контроля записей
    
    RecordCountConfig() : enabled(false) {}
    
    RecordCountConfig(const std::string& xp, const std::string& attr, bool en = true)
        : xpath(xp), attribute(attr), enabled(en) {}
};

/**
 * @struct SourceConfig
 * @brief Хранит всю конфигурацию одного источника данных
 *
 * @details
 * Структура содержит обязательные поля для подключения к хранилищу:
 * `name`, `type`, `path`, `file_mask`, `processed_dir`, а также
 * опциональные параметры ошибок, фильтрации и соединения.
 * Обеспечивает методы сериализации в JSON и валидации параметров.
 */
struct SourceConfig {
  // ============= Обязательные поля =============

  /**
   * @brief Уникальное имя источника данных.
   *
   * @details
   * Используется для идентификации источника в системе, логах и метриках.
   * Должно быть уникальным среди всех источников в конфигурации.
   *
   * @warning Свойство является обязательным в конфигурации источника
   */
  std::string name;

  /**
   * @brief Тип хранилища (local, smb, ftp).
   *
   * @details
   * Определяет, какой адаптер будет использоваться для подключения к источнику.
   * Поддерживаются значения: "local" (локальная файловая система), "smb"
   * (сетевой ресурс), "ftp" (FTP-сервер). Влияет на набор обязательных
   * параметров в params.
   *
   * @warning Свойство является обязательным в конфигурации источника
   */
  std::string type;

  /**
   * @brief Путь к источнику данных.
   *
   * @details
   * Для локальных источников — абсолютный или относительный путь к директории.
   * Для smb/ftp — путь к удаленному ресурсу, например, //server/share или
   * ftp://host/dir.
   *
   * @warning Свойство является обязательным в конфигурации источника
   */
  std::string path;

  /**
   * @brief Маска файлов для обработки (поддерживает wildcards *, ?).
   *
   * @details
   * Определяет, какие файлы из директории будут рассматриваться для обработки.
   * Например, "*.xml" — только XML-файлы, "report_*.csv" — только отчеты.
   *
   * @warning Свойство является обязательным в конфигурации источника
   */
  std::string file_mask;

  /**
   * @brief Директория для обработанных файлов.
   *
   * @details
   * После успешной обработки файл перемещается в указанную директорию.
   * Для каждого источника может быть задана своя директория.
   *
   * @warning Свойство является обязательным в конфигурации источника
   */
  std::string processed_dir;

  // ============= Опциональные поля =============

  /**
   * @brief Директория для файлов с ошибками.
   *
   * @details
   * Если при обработке файла возникла ошибка (например, файл повреждён или не
   * соответствует формату), он перемещается в эту директорию для последующего
   * анализа. Может быть пустой, тогда файлы с ошибками не перемещаются.
   *
   * @note Свойство является опциональным в конфигурации источника
   */
  std::string bad_dir;

  /**
   * @brief Директория для файлов исключённых данных.
   *
   * @details
   * Данные, которые были исключены фильтрами (например, попали в стоп-лист),
   * перемещаются в файлы создаваймые в этой директории. Используется для
   * дальнейшего анализа или хранения исключённых данных.
   *
   * @note Свойство является опциональным в конфигурации источника
   */
  std::string excluded_dir;

  /**
   * @brief Шаблон имени для отфильтрованных файлов.
   *
   * @details
   * Формирует имя файла при перемещении в processed_dir.
   * Поддерживает плейсхолдеры {filename} (имя без расширения) и {ext}
   * (расширение). Например: "{filename}_filtered.{ext}".
   *
   * @note Свойство является опциональным в конфигурации источника
   */
  std::string filtered_template = "{filename}_filtered.{ext}";

  /**
   * @brief Шаблон имени для исключённых файлов.
   *
   * @details
   * Формирует имя файла при перемещении в excluded_dir.
   * Поддерживает плейсхолдеры {filename} (имя без расширения) и {ext}
   * (расширение). Например: "{filename}_excluded.{ext}".
   *
   * @note Свойство является опциональным в конфигурации источника
   */
  std::string excluded_template = "{filename}_excluded.{ext}";

  /**
   * @brief Путь к файлу сравнения для фильтрации.
   *
   * @details
   * Указывает на CSV-файл, содержащий список значений для фильтрации (например,
   * стоп-лист документов). Используется при сравнении значений из XML с
   * внешними списками. По умолчанию: "./comparison_list.csv".
   *
   * @note Свойство не используется если filtering_enabled == false.
   */
  std::string comparison_list = "./comparison_list.csv";

  /**
   * @brief Флаг включения фильтрации.
   *
   * @details
   * Если установлен в false, фильтрация XML-файлов по критериям не выполняется,
   * все файлы считаются "чистыми".
   */
  bool filtering_enabled = true;

  /**
   * @brief Интервал проверки изменений (в секундах).
   *
   * @details
   * Определяет, с какой периодичностью источник будет проверять наличие новых
   * файлов для обработки. Значение по умолчанию — 5 секунд.
   */
  std::chrono::seconds check_interval{5};

  /**
   * @brief Флаг активности источника.
   *
   * @details
   * Если установлен в false, источник игнорируется при запуске сервиса.
   * Позволяет временно отключать обработку без удаления конфигурации.
   */
  bool enabled = true;

  // ============= Параметры подключения =============

  /**
   * @brief Параметры подключения (username, password, port и т.п.)
   * @details
   * Словарь параметров подключения для SMB/FTP:
   * - `username`: имя пользователя (in)
   * - `password`: пароль (in)
   * - `domain`: домен или рабочая группа (SMB)
   * - `port`: порт (FTP, optional)
   * - `timeout`: таймаут соединения (optional)
   */
  std::unordered_map<std::string, std::string> params;

  /**
   * @struct XmlNamespace
   * @brief Описывает пространство имён для XPath-запросов
   * @ingroup XMLFiltering
   *
   * @details
   * Регистрирует префикс и URI пространства имён XML для корректной работы
   * XPath-выражений в документах с использованием неймспейсов. Обязателен для
   * документов, где элементы принадлежат разным неймспейсам.
   */
  struct XmlNamespace {
    /**
     * @brief Префикс пространства имён
     *
     * @details
     * Короткий идентификатор, используемый в XPath-выражениях.
     * Примеры: "ns1", "xs", "xsi".
     *
     * @note Должен соответствовать префиксу в XML-документе
     * @warning Не должен содержать двоеточий или пробелов
     */
    std::string prefix;

    /**
     * @brief URI пространства имён
     *
     * @details
     * Уникальный идентификатор пространства имён, объявленный в XML.
     * Пример: "http://www.w3.org/2001/XMLSchema-instance"
     *
     * @note Должен точно соответствовать значению xmlns:* в докуменte
     */
    std::string uri;
  };

  /**
   * @struct XmlFilterCriterion
   * @brief Описывает один критерий фильтрации XML-документов
   * @ingroup XMLFiltering
   *
   * @details
   * Определяет правило для сравнения значений из XML-файла с данными в CSV.
   * Поддерживает извлечение значений из атрибутов или текстового содержимого
   * узлов.
   */
  struct XmlFilterCriterion {
    /**
     * @brief XPath-выражение для поиска узлов
     *
     * @details
     * Путь для поиска узлов в XML-документе. Поддерживает стандартный
     * XPath 1.0. Примеры:
     * - "//book/title" - все элементы title внутри book
     * - "/catalog/book[1]/@id" - атрибут id первой книги
     *
     * @note Может содержать префиксы пространств имён
     * @warning Невалидный XPath приведёт к ошибке фильтрации
     */
    std::string xpath;

    /**
     * @brief Имя атрибута для извлечения значения
     *
     * @details
     * Если указан, извлекает значение атрибута вместо текстового содержимого
     * узла. Пример: для узла <book id="123"> значение атрибута "id" будет
     * "123".
     *
     * @note Оставьте пустым для использования текстового содержимого узла
     */
    std::string attribute;

    /**
     * @brief Имя столбца в CSV для сравнения
     *
     * @details
     * Название столбца в comparison_list CSV, с которым будет сравниваться
     * извлечённое значение. Пример: "document_id".
     *
     * @note Столбец должен существовать в CSV-файле
     */
    std::string csv_column;

    /**
     * @brief Флаг обязательности критерия
     *
     * @details
     * Если true, отсутствие узла или несовпадение значения приведёт к
     * исключению данных из файла. Если false, критерий учитывается только при
     * наличии узла.
     *
     * @note Для OR-логики обычно устанавливается false
     */
    bool required = true;

    /**
     * @brief Вес критерия для взвешенной логики
     *
     * @details
     * Используется только при logic_operator = "WEIGHTED". Определяет вклад
     * критерия в общую оценку. Пример: 1.0 - стандартный вес, 2.0 - двойной
     * вклад.
     *
     * @note Для других типов операторов игнорируется
     * @warning Должен быть > 0
     */
    double weight = 1.0;
  };

  /**
   * @struct XmlFilterConfig
   * @brief Конфигурация системы фильтрации XML-документов
   * @ingroup XMLFiltering
   *
   * @details
   * Агрегирует все параметры для обработки XML-файлов: критерии фильтрации,
   * логику объединения результатов, пространства имён и пороговые значения.
   */
  struct XmlFilterConfig {
    /**
     * @brief Список критериев фильтрации
     *
     * @details
     * Вектор индивидуальных критериев, применяемых последовательно к документу.
     * Минимальное количество - 1.
     *
     * @note Порядок критериев влияет на производительность, но не на результат
     */
    std::vector<XmlFilterCriterion> criteria;

    /**
     * @brief Логический оператор объединения результатов
     *
     * @details
     * Определяет, как комбинировать результаты отдельных критериев:
     * - "AND": все критерии должны вернуть true (по умолчанию)
     * - "OR": хотя бы один критерий должен вернуть true
     * - "MAJORITY": большинство критериев (>50%) должны вернуть true
     * - "WEIGHTED": взвешенная сумма превышает порог
     *
     * @note Поддерживаются только указанные значения
     */
    std::string logic_operator = "AND";

    /**
     * @brief Путь к CSV-файлу для сравнения
     *
     * @details
     * Переопределяет глобальный comparison_list для этой конкретной
     * конфигурации. Если пусто, используется значение из корневого уровня
     * SourceConfig.
     */
    std::string comparison_list;

    /**
     * @brief Порог для операторов MAJORITY и WEIGHTED
     *
     * @details
     * - Для MAJORITY: доля совпавших критериев (0.5 = 50%)
     * - Для WEIGHTED: минимальная взвешенная сумма (0.0-1.0)
     *
     * @note Для AND/OR игнорируется
     * @warning Должен быть в диапазоне (0.0, 1.0]
     */
    double threshold = 0.5;

    /**
     * @brief Список пространств имён для XPath
     *
     * @details
     * Регистрирует пользовательские пространства имён до выполнения
     * XPath-запросов. Требуется для документов с xmlns-декларациями.
     *
     * @note Автоподстановка из документа может отключить необходимость ручной
     * регистрации
     */
    std::vector<XmlNamespace> namespaces;

    /**
     * @brief Флаг автоматической регистрации пространств имён
     *
     * @details
     * Если true, парсер автоматически регистрирует все пространства имён,
     * объявленные в корневом элементе документа. Упрощает конфигурацию,
     * но может замедлить обработку для больших документов.
     *
     * @note Ручная регистрация через namespaces имеет приоритет
     */
    bool auto_register_namespaces = true;

      /**
       * @brief Параметры контроля количества записей в файле
       */
    RecordCountConfig record_count_config;
  } xml_filter;

  /**
     * @brief Создаёт объект SourceConfig из JSON
     * @ingroup Serialization
     *
     * @param[in] src JSON с настройками источника
     * @return SourceConfig Объект с заполненными полями
     * @throw std::runtime_error При отсутствии обязательных полей или неверном
     типе
     *
     * @details
     * Выполняет проверку наличия "name", "type", "path", "file_mask",
     * "processed_dir" и парсит остальные опциональные поля.
     *
     * @note Парсит check_interval как целое число секунд, fallback = 5
     *
     * @code
     nlohmann::json js = loadJson("source.json");
     SourceConfig cfg = SourceConfig::fromJson(js);
     @endcode
     */
  static SourceConfig fromJson(const nlohmann::json &src);

  /**
     * @brief Конвертирует текущую конфигурацию в JSON
     * @ingroup Serialization
     *
     * @return nlohmann::json JSON-объект с параметрами источника
     *
     * @details
     * Сериализует все поля, включая секцию xml_filter. Не включает
     * пустые optional-поля.
     *
     * @code
     SourceConfig cfg = ...;
     nlohmann::json js = cfg.toJson();
     @endcode
     */
  nlohmann::json toJson() const;

  /**
     * @brief Проверяет корректность полей конфигурации
     * @ingroup Validation
     *
     * @throw std::invalid_argument При пустых обязательных полях или
     некорректных значениях
     *
     * @details
     * Проверяет:
     *  - name, type, path, file_mask, processed_dir != ""
     *  - type ∈ {"local","smb","ftp"}
     *  - Для SMB и FTP проверяет обязательные params
     *  - check_interval > 0
     *  - Если filtering_enabled, то xml_filter.criteria ≠ пусто
     *  - Вес > 0, threshold ∈ (0.0,1.0]
     *
     * @code
     cfg.validate();
     @endcode
     *
     * @note Используется после fromJson() для гарантии целостности
     * @warning Изменение структуры требует обновления валидации
     */
  void validate() const;

  /**
     * @brief Генерирует имя для отфильтрованного файла
     * @ingroup Utilities
     *
     * @param[in] original_filename Исходное имя файла (без пути)
     * @return std::string Новое имя по шаблону filtered_template
     *
     * @details
     * Заменяет `{filename}` и `{ext}` на части имени original_filename.
     * Использует std::filesystem для разделения имени и расширения.
     *
     * @code
     cfg.filtered_template = "{filename}_ok.{ext}";
     std::string fn = cfg.getFilteredFileName("data.xml"); // => "data_ok.xml"
     @endcode
     */
  std::string getFilteredFileName(const std::string &original_filename) const;

  /**
     * @brief Генерирует имя для исключённого файла
     * @ingroup Utilities
     *
     * @param[in] original_filename Исходное имя файла (без пути)
     * @return std::string Новое имя по шаблону excluded_template
     *
     * @details
     * Идентична getFilteredFileName, но использует шаблон excluded_template.
     *
     * @code
     cfg.excluded_template = "{filename}_excl.{ext}";
     std::string fn = cfg.getExcludedFileName("data.xml"); // => "data_excl.xml"
     @endcode
     */
  std::string getExcludedFileName(const std::string &original_filename) const;

  /**
   * @brief Проверяет наличие обязательных параметров подключения
   * @ingroup Validation
   *
   * @param[in] required_params Список необходимых ключей в params
   * @return bool true если все ключи присутствуют и непустые
   *
   * @details
   * Проходит по required_params и проверяет, что params[key] существует
   * и не является пустой строкой.
   *
   * @code
   * bool ok = cfg.hasRequiredParams({"username","password"});
   * @endcode
   */
  bool hasRequiredParams(const std::vector<std::string> &required_params) const;

 private:
  /**
   * @brief Применяет строковый шаблон к имени файла
   * @ingroup Internal
   *
   * @param[in] filename Имя файла без пути (in)
   * @param[in] template_str Шаблон с плейсхолдерами `{filename}`, `{ext}` (in)
   * @return std::string Сформированное имя файла
   *
   * @details
   * Разделяет filename на имя и расширение с помощью std::filesystem,
   * затем заменяет в template_str.
   *
   * @warning Не проверяет существование файла в файловой системе.
   */
  std::string applyTemplate(const std::string &filename,
                            const std::string &template_str) const;
};