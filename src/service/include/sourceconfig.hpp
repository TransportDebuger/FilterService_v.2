/**
 * @file SourceConfig.hpp
 * @brief Конфигурация источника данных для различных типов хранилищ
 *
 * @details Определяет структуру конфигурации для работы с различными
 * типами файловых хранилищ (локальные, SMB, FTP) с поддержкой
 * параметров подключения, валидации и фильтрации XML
 *
 * Ключевые особенности реализации:
 * Многокритериальная фильтрация
 * Поддержка нескольких критериев фильтрации через массив criteria
 * Каждый критерий содержит XPath-выражение, атрибут и столбец CSV для сравнения
 * Возможность указания обязательности критерия через флаг required
 *
 * Логические операторы
 * Поддержка четырех типов логических операторов:
 * AND - все критерии должны совпадать
 * OR - достаточно одного совпадения
 * MAJORITY - большинство критериев должны совпадать
 * WEIGHTED - взвешенная сумма превышает порог
 */

#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @struct SourceConfig
 * @brief Структура конфигурации источника данных
 *
 * @note Поддерживает различные типы хранилищ через параметры подключения
 */
struct SourceConfig {
  // ============= Обязательные поля =============

  /// Уникальное имя источника данных
  std::string name;

  /// Тип хранилища (local, smb, ftp)
  std::string type;

  /// Путь к источнику данных
  std::string path;

  /// Маска файлов для обработки (поддерживает wildcards *, ?)
  std::string file_mask;

  /// Директория для обработанных файлов
  std::string processed_dir;

  // ============= Опциональные поля =============

  /// Директория для файлов с ошибками
  std::string bad_dir;

  /// Директория для исключенных файлов
  std::string excluded_dir;

  /// Шаблон имени для отфильтрованных файлов
  std::string filtered_template = "{filename}_filtered.{ext}";

  /// Шаблон имени для исключенных файлов
  std::string excluded_template = "{filename}_excluded.{ext}";

  /// Путь к файлу сравнения для фильтрации
  std::string comparison_list = "./comparison_list.csv";

  /// Флаг включения фильтрации
  bool filtering_enabled = true;

  /// Интервал проверки изменений (в секундах)
  std::chrono::seconds check_interval{5};

  /// Флаг активности источника
  bool enabled = true;

  // ============= Параметры подключения =============

  /**
   * @brief Параметры подключения для различных типов хранилищ
   *
   * @details Поддерживаемые параметры:
   * - username: имя пользователя (SMB, FTP)
   * - password: пароль (SMB, FTP)
   * - domain: домен/workgroup (SMB)
   * - port: порт подключения (FTP)
   * - timeout: таймаут соединения (FTP, SMB)
   */
  std::unordered_map<std::string, std::string> params;

  // ============= Конфигурация фильтрации XML =============
  struct XmlNamespace {
    std::string prefix;
    std::string uri;
  };

  /**
   * @brief Критерий фильтрации XML
   * @details Определяет один критерий для фильтрации XML-файлов
   */
  struct XmlFilterCriterion {
    std::string xpath;       // XPath для поиска узлов
    std::string attribute;   // Имя атрибута (опционально)
    std::string csv_column;  // Столбец CSV для сравнения
    bool required = true;    // Обязательность критерия
    double weight = 1.0;     // Вес критерия (для WEIGHTED)
  };

  /**
   * @brief Конфигурация фильтрации XML
   * @details Определяет параметры фильтрации XML-файлов
   */
  struct XmlFilterConfig {
    std::vector<XmlFilterCriterion> criteria;  // Критерии фильтрации
    std::string logic_operator = "AND";  // AND, OR, MAJORITY, WEIGHTED
    std::string comparison_list;
    double threshold = 0.5;  // Порог для MAJORITY и WEIGHTED (0.0-1.0)
    std::vector<XmlNamespace> namespaces;
    bool auto_register_namespaces =
        true;  // Автоматическая регистрация из документа
  } xml_filter;

  // ============= Методы =============

  /**
   * @brief Создает SourceConfig из JSON объекта
   * @param src JSON объект с конфигурацией
   * @return SourceConfig Настроенная структура конфигурации
   * @throw std::runtime_error При отсутствии обязательных полей
   */
  static SourceConfig fromJson(const nlohmann::json &src);

  /**
   * @brief Преобразует SourceConfig в JSON
   * @return nlohmann::json JSON представление конфигурации
   */
  nlohmann::json toJson() const;

  /**
   * @brief Валидирует конфигурацию источника
   * @throw std::invalid_argument При невалидной конфигурации
   */
  void validate() const;

  /**
   * @brief Получает имя отфильтрованного файла
   * @param original_filename Исходное имя файла
   * @return std::string Имя файла по шаблону filtered_template
   */
  std::string getFilteredFileName(const std::string &original_filename) const;

  /**
   * @brief Получает имя исключенного файла
   * @param original_filename Исходное имя файла
   * @return std::string Имя файла по шаблону excluded_template
   */
  std::string getExcludedFileName(const std::string &original_filename) const;

  /**
   * @brief Проверяет наличие обязательных параметров для типа хранилища
   * @param required_params Список обязательных параметров
   * @return bool true если все параметры присутствуют
   */
  bool hasRequiredParams(const std::vector<std::string> &required_params) const;

 private:
  /**
   * @brief Применяет шаблон к имени файла
   * @param filename Исходное имя файла
   * @param template_str Строка шаблона
   * @return std::string Результирующее имя файла
   */
  std::string applyTemplate(const std::string &filename,
                            const std::string &template_str) const;
};