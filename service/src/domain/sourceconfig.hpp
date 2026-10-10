/**
 * @file sourceconfig.hpp
 * @brief Строгие структуры данных для конфигурации источника.
 * @version 2.1.0
 * @date 2026-09-06
 */
#pragma once
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
namespace stc {

/**
 * @struct SourceConfig
 * @brief Конфигурация одного источника данных для обработки.
 */
struct SourceConfig {
    // --- Обязательные поля ---

    /// @brief Уникальное имя источника.
    std::string name;
    /// @brief Тип хранилища: "local", "smb", "ftp".
    std::string type;
    /// @brief Путь к источнику данных.
    std::string path;
    /// @brief Маска файлов (glob: *, ?).
    std::string file_mask;
    /// @brief Директория для обработанных файлов.
    std::string processed_dir;

    // --- Опциональные поля ---

    /// @brief Директория для файлов с ошибками.
    std::string bad_dir;
    /// @brief Директория для исключённых данных.
    std::string excluded_dir;
    /// @brief Директория для временных файлов.
    std::string temp_dir;
    /// @brief Шаблон имени отфильтрованного файла.
    std::string filtered_template =
        "{filename}_filtered.{ext}";
    /// @brief Шаблон имени исключённого файла.
    std::string excluded_template =
        "{filename}_excluded.{ext}";
    /// @brief Путь к CSV-файлу сравнения.
    std::string comparison_list =
        "./comparison_list.csv";
    /// @brief Флаг включения фильтрации.
    bool filtering_enabled = true;
    /// @brief Интервал проверки изменений.
    std::chrono::seconds check_interval{5};
    /// @brief Флаг активности источника.
    bool enabled = true;
    /// @brief Стратегия мониторинга ФС: "auto", "inotify", "polling".
    std::string monitoring_strategy =
        "auto";

    // --- Параметры подключения ---

    /// @brief Параметры подключения (username, password, domain, port).
    std::unordered_map<std::string, std::string>
        params;

    /**
     * @struct RecordCountConfig
     * @brief Параметры контроля количества записей в XML-документе.
     */
    struct RecordCountConfig {
        /// @brief Путь к счётчику относительно корневого элемента.
        std::string path;
        /// @brief Флаг активности контроля.
        bool enabled{false};
        /// @brief Конструктор по умолчанию.
        RecordCountConfig() = default;

        /**
         * @brief Параметризованный конструктор.
         *
         * @param[in] p Путь к счётчику.
         * @param[in] en Флаг активности.
         */
        explicit RecordCountConfig(std::string p, bool en = true)
            : path(std::move(p)), enabled(en) {}
    };

    /**
     * @struct XmlNamespace
     * @brief Пространство имён для XPath-запросов.
     */
    struct XmlNamespace {
        /// @brief Префикс пространства имён.
        std::string prefix;
        /// @brief URI пространства имён.
        std::string uri;
    };

    /**
     * @struct XmlFilterCriterion
     * @brief Один критерий фильтрации XML-документа.
     */
    struct XmlFilterCriterion {
        /// @brief Путь к свойству относительно объекта или группы.
        std::string path;
        /// @brief Имя атрибута для извлечения значения.
        std::string attribute;
        /// @brief Имя столбца CSV для сравнения.
        std::string csv_column;
        /// @brief Флаг активности критерия.
        bool required = true;
        /// @brief Вес критерия (для WEIGHTED-логики).
        double weight = 1.0;
    };

    /**
     * @struct XmlFilterConfig
     * @brief Агрегированная конфигурация фильтрации XML.
     */
    struct XmlFilterConfig {
        /// @brief Имя тега объекта фильтрации.
        std::string object_name;
        //// @brief URI пространства имён объекта.
        std::string object_namespace_uri;
        /// @brief Список критериев фильтрации.
        std::vector<XmlFilterCriterion> criteria;
        /// @brief Логический оператор: "AND", "OR", "MAJORITY", "WEIGHTED".
        std::string logic_operator = "AND";
        /// @brief Переопределение пути к CSV для данной конфигурации.
        std::string comparison_list;
        /// @brief Порог для WEIGHTED-логики.
        double threshold = 0.5;
        /// @brief Пространства имён для XPath-запросов.
        std::vector<XmlNamespace> namespaces;
        /// @brief Автоматическая регистрация пространств имён.
        bool auto_register_namespaces = true;
        /// @brief Параметры контроля количества записей.
        RecordCountConfig record_count_config;
    };

    /// @brief Конфигурация XML-фильтрации.
    XmlFilterConfig xml_filter;

    /**
     * @brief Десериализует SourceConfig из JSON-объекта.
     *
     * @param[in] src JSON с настройками источника.
     * @return SourceConfig Заполненная структура конфигурации.
     * @throw std::runtime_error При отсутствии обязательных полей.
     */
    static SourceConfig fromJson(const nlohmann::json& src);

    /**
     * @brief Сериализует конфигурацию в JSON-объект.
     *
     * @return nlohmann::json JSON-представление конфигурации.
     */
    nlohmann::json toJson() const;

    /**
     * @brief Проверяет корректность всех полей конфигурации.
     *
     * @throw std::invalid_argument При пустых обязательных полях.
     */
    void validate() const;

    /**
     * @brief Генерирует имя отфильтрованного файла по шаблону.
     *
     * @param[in] original_filename Исходное имя файла.
     * @return std::string Новое имя файла.
     */
    std::string getFilteredFileName(const std::string& original_filename) const;

    /**
     * @brief Генерирует имя исключённого файла по шаблону.
     *
     * @param[in] original_filename Исходное имя файла.
     * @return std::string Новое имя файла.
     */
    std::string getExcludedFileName(const std::string& original_filename) const;

    /**
     * @brief Проверяет наличие обязательных параметров подключения.
     *
     * @param[in] required_params Список ключей для проверки.
     * @return true Если все ключи присутствуют и непустые.
     */
    bool hasRequiredParams(const std::vector<std::string>& required_params) const;

private:
    /**
     * @brief Применяет строковый шаблон к имени файла.
     *
     * @param[in] filename Имя файла без пути.
     * @param[in] template_str Шаблон с плейсхолдерами.
     * @return std::string Сформированное имя файла.
     * @private
     */
    std::string applyTemplate(const std::string& filename,
                              const std::string& template_str) const;
};
}  // namespace stc