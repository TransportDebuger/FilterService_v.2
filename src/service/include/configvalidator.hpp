/**
 * @file configvalidator.hpp
 * @author Artem Ulyanov
 * @version 1
 * @date May, 2025
 * @brief Заголовочный файл класса ConfigValidator
 * @details Заголовочный файл класса CofnfigValidator. Содержит объявление
 * скласса, его свойств и методов.
 */

#pragma once
#include <nlohmann/json.hpp>

/**
 * @class ConfigValidator
 * @brief Класс для валидации структуры JSON-конфигурации
 *
 * @note Проверяет:
 * - Наличие обязательных секций
 * - Корректность типов данных
 * - Специфичные ограничения для разных типов источников
 */
class ConfigValidator {
 public:
  /**
   * @brief Валидирует корневую структуру конфига
   * @param config Ссылка на JSON-объект конфигурации
   * @return true Если структура валидна
   * @throw std::runtime_error При нарушении структуры
   */
  bool validateRoot(const nlohmann::json& config) const;

  /**
   * @brief Валидирует секцию источников
   * @param sources JSON-массив источников
   * @return true Если все источники валидны
   * @throw std::runtime_error При ошибках в конфигурации источников
   */
  bool validateSources(const nlohmann::json& sources) const;

  /**
   * @brief Валидирует настройки логирования
   * @param logging JSON-объект секции логирования
   * @return true Если настройки корректны
   * @throw std::runtime_error При недопустимых значениях
   */
  bool validateLogging(const nlohmann::json& logging) const;

 private:
  /**
   * @brief Проверяет обязательные поля для FTP/SFTP источников
   * @param source JSON-объект источника
   */
  void validateFtpFields(const nlohmann::json& source) const;
};