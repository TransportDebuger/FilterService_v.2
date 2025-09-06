/**
 * @file configvalidator.hpp
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 * @brief Валидация структуры JSON-конфигурации
 *
 * @details
 * Класс **ConfigValidator** проверяет структуру и содержимое
 * JSON-конфигурации XML Filter Service на соответствие спецификации:
 * - корневые секции `"defaults"` и `"environments"`
 * - массив `"sources"` с обязательными полями
 * - секция `"logging"` с корректными типами и параметрами логгеров
 *
 * Реализует паттерн «Validator» для централизованной проверки
 * конфигурационных файлов до их применения.
 *
 * @version 1.0
 * @since 1.0
 */

#pragma once

#include <nlohmann/json.hpp>

/**
 * @defgroup Configuration Компоненты управления конфигурацией
 */

/**
 * @defgroup ValidationMethods Методы валидации данных
 */

/**
 * @defgroup InternalMethods Внутренние (приватные) методы классов
 */

/**
 * @class ConfigValidator
 * @brief Предоставляет методы валидации JSON-конфига сервиса
 * @ingroup Configuration
 *
 * @details
 * Класс выполняет проверку структуры и типов данных в конфигурационном
 * JSON-файле сервиса, выбрасывая исключения при нарушении требований.
 * Проверяет:
 * - Наличие обязательных секций
 * - Корректность типов данных
 * - Специфичные ограничения для разных типов источников
 *
 * @see ConfigLoader, ConfigCache
 */
class ConfigValidator {
 public:
  /**
     * @brief Проверяет наличие обязательных корневых секций
     * @ingroup ValidationMethods
     *
     * @details
     * Убеждается, что JSON содержит незаполненные объекты
     * `"defaults"` и `"environments"`. Раздел `"defaults"` не должен быть
     пустым.
     *
     * @param[in] config JSON-объект конфигурации
     * @return true Если обе секции присутствуют и корректны
     * @throw std::runtime_error При отсутсвии или некорректном типе секции
     *
     * @note Секция `"defaults"` не может быть пустой
     * @warning Корректное выполнение этого метода обязательно перед остальными
     проверками
     *
     * @code
     ConfigValidator v;
     if (v.validateRoot(jsonConfig)) {
         // Можно проверять дальше
     }
     @endcode
     */
  bool validateRoot(const nlohmann::json &config) const;

  /**
     * @brief Проверяет массив источников файлов
     * @ingroup ValidationMethods
     *
     * @details
     * Убеждается, что параметр `"sources"` является массивом объектов,
     * каждый из которых содержит обязательные поля:
     * `"name"`, `"type"`, `"path"`, `"file_mask"`, `"processed_dir"`.
     *
     * @param[in] sources JSON-массив объектов-источников
     * @return true Если все элементы массива валидны
     * @throw std::runtime_error При некорректном формате или отсутствии полей
     *
     * @note Для FTP/SFTP дополнительно проверяются поля `"username"` и
     `"password"`,
     * а поле `"port"` должно быть числом, если задано.
     * @warning Любой неверный источник приведёт к исключению
     *
     * @code
     auto srcArray = jsonConfig["environments"]["production"]["sources"];
     ConfigValidator v;
     v.validateSources(srcArray);
     @endcode
     */
  bool validateSources(const nlohmann::json &sources) const;

  /**
     * @brief Проверяет секцию логирования конфигурации
     * @ingroup ValidationMethods
     *
     * @details
     * Убеждается, что `"logging"` является массивом объектов с корректными
     * полями: `"type"` (строка в {console, sync_file, async_file}),
     * `"level"` (строка), и для файловых логгеров — `"file"` (строка).
     *
     * @param[in] logging JSON-массив конфигураций логгеров
     * @return true Если все записи корректны
     * @throw std::runtime_error При нарушении структуры или типов
     *
     * @note Допустимые типы логгеров: console, sync_file, async_file
     * @warning Некорректный уровень или отсутствие пути для file-логгера —
     ошибка
     *
     * @code
     auto logArray = jsonConfig["environments"]["production"]["logging"];
     ConfigValidator v;
     v.validateLogging(logArray);
     @endcode
     */
  bool validateLogging(const nlohmann::json &logging) const;

 private:
  /**
   * @brief Проверяет обязательные поля FTP/SFTP-источника
   * @ingroup InternalMethods
   *
   * @details
   * Убеждается, что JSON-объект источника содержит
   * поля `"username"` и `"password"` (строки). Если указан `"port"`,
   * оно должно быть числом.
   *
   * @param[in] source JSON-объект одного источника
   * @throw std::runtime_error При отсутствии или неверном типе полей
   *
   * @note Используется внутри validateSources для типов ftp/sftp
   *
   * @code
   * ConfigValidator v;
   * v.validateFtpFields(jsonConfig["environments"]["prod"]["sources"][i]);
   * @endcode
   */
  void validateFtpFields(const nlohmann::json &source) const;
};