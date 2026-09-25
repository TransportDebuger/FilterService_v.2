/**
@file sourceconfig.hpp
@brief Строгие структуры данных для конфигурации источника.
@version 2.1.0
@date 2026-09-06
*/
#pragma once
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
namespace stc {
/**
@struct SourceConfig
@brief Конфигурация одного источника данных для обработки.
*/
struct SourceConfig {
    // --- Обязательные поля ---
    std::string name;       ///< Уникальное имя источника.
    std::string type;       ///< Тип хранилища: "local", "smb", "ftp".
    std::string path;       ///< Путь к источнику данных.
    std::string file_mask;  ///< Маска файлов (glob: *, ?).
    std::string processed_dir;  ///< Директория для обработанных файлов.

    // --- Опциональные поля ---
    std::string bad_dir;  ///< Директория для файлов с ошибками.
    std::string excluded_dir;  ///< Директория для исключённых данных.
    std::string temp_dir;  ///< Директория для временных файлов.
    std::string filtered_template =
        "{filename}_filtered.{ext}";  ///< Шаблон имени отфильтрованного файла.
    std::string excluded_template =
        "{filename}_excluded.{ext}";  ///< Шаблон имени исключённого файла.
    std::string comparison_list =
        "./comparison_list.csv";  ///< Путь к CSV-файлу сравнения.
    bool filtering_enabled = true;  ///< Флаг включения фильтрации.
    std::chrono::seconds check_interval{5};  ///< Интервал проверки изменений.
    bool enabled = true;  ///< Флаг активности источника.
    std::string monitoring_strategy =
        "auto";  ///< Стратегия мониторинга ФС: "auto", "inotify", "polling".

    // --- Параметры подключения ---
    std::unordered_map<std::string, std::string>
        params;  ///< Параметры подключения (username, password, domain, port).

    /**
    @struct RecordCountConfig
    @brief Параметры контроля количества записей в XML-документе.
    */
    struct RecordCountConfig {
        /// @brief Путь к счётчику относительно корневого элемента.
        /// Примеры: "@recordCount" для атрибута корня,
        ///          "portion" для вложенного элемента,
        ///          "stats/portion" для вложенного элемента с промежуточным уровнем.
        std::string path;
        /// @brief Флаг активности контроля.
        bool enabled{false};
        RecordCountConfig() = default;
        explicit RecordCountConfig(std::string p, bool en = true)
            : path(std::move(p)), enabled(en) {}
    };

    /**
    @struct XmlNamespace
    @brief Пространство имён для XPath-запросов.
    */
    struct XmlNamespace {
        std::string prefix;  ///< Префикс пространства имён.
        std::string uri;     ///< URI пространства имён.
    };

    /**
    @struct XmlFilterCriterion
    @brief Один критерий фильтрации XML-документа.
    */
    struct XmlFilterCriterion {
        /// @brief Путь к свойству относительно объекта или группы.
        /// Примеры: "docNumber", "docNumber/@value", "@id",
        ///          "ancestor::aircompany/@name", "ancestor::category/ancestor::subcategory/@name".
        std::string path;
        /// @brief Имя атрибута для извлечения значения (пусто = текстовое содержимое).
        std::string attribute;
        /// @brief Имя столбца CSV для сравнения.
        std::string csv_column;
        /// @brief Флаг активности критерия. Если false, критерий исключается из оценки.
        bool required = true;
        /// @brief Вес критерия (для WEIGHTED-логики).
        double weight = 1.0;
    };

    /**
    @struct XmlFilterConfig
    @brief Агрегированная конфигурация фильтрации XML.
    */
    struct XmlFilterConfig {
        /// @brief Имя тега объекта фильтрации.
        std::string object_name;
        /// @brief URI пространства имён объекта (опционально).
        /// Если пусто, объект распознаётся по локальному имени независимо от пространства имён.
        /// Если задано, объект распознаётся только по сочетанию локального имени и пространства имён.
        std::string object_namespace_uri;
        /// @brief Список критериев фильтрации.
        std::vector<XmlFilterCriterion> criteria;
        /// @brief Логический оператор: "AND", "OR", "MAJORITY", "WEIGHTED".
        std::string logic_operator = "AND";
        /// @brief Переопределение пути к CSV для данной конфигурации.
        std::string comparison_list;
        /// @brief Порог для WEIGHTED-логики. Игнорируется для остальных операторов.
        double threshold = 0.5;
        /// @brief Пространства имён для XPath-запросов.
        std::vector<XmlNamespace> namespaces;
        /// @brief Автоматическая регистрация пространств имён из документа.
        bool auto_register_namespaces = true;
        /// @brief Параметры контроля количества записей.
        RecordCountConfig record_count_config;
    };

    /// @brief Конфигурация XML-фильтрации.
    XmlFilterConfig xml_filter;

    /**
    @brief Десериализует SourceConfig из JSON-объекта.
    @param[in] src JSON с настройками источника.
    @return SourceConfig Заполненная структура конфигурации.
    @throw std::runtime_error При отсутствии обязательных полей или неверных типах.
    */
    static SourceConfig fromJson(const nlohmann::json& src);

    /**
    @brief Сериализует конфигурацию в JSON-объект.
    @return nlohmann::json JSON-представление конфигурации.
    */
    nlohmann::json toJson() const;

    /**
    @brief Проверяет корректность всех полей конфигурации.
    @throw std::invalid_argument При пустых обязательных полях или некорректных значениях.
    */
    void validate() const;

    /**
    @brief Генерирует имя отфильтрованного файла по шаблону.
    @param[in] original_filename Исходное имя файла.
    @return std::string Новое имя файла.
    */
    std::string getFilteredFileName(const std::string& original_filename) const;

    /**
    @brief Генерирует имя исключённого файла по шаблону.
    @param[in] original_filename Исходное имя файла.
    @return std::string Новое имя файла.
    */
    std::string getExcludedFileName(const std::string& original_filename) const;

    /**
    @brief Проверяет наличие обязательных параметров подключения.
    @param[in] required_params Список ключей, которые должны присутствовать в params.
    @return true Если все ключи присутствуют и непустые.
    */
    bool hasRequiredParams(const std::vector<std::string>& required_params) const;

private:
    /**
    @private
    @brief Применяет строковый шаблон к имени файла.
    @param[in] filename Имя файла без пути.
    @param[in] template_str Шаблон с плейсхолдерами {filename} и {ext}.
    @return std::string Сформированное имя файла.
    */
    std::string applyTemplate(const std::string& filename,
                              const std::string& template_str) const;
};
}  // namespace stc