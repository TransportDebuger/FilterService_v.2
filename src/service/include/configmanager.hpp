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
 * @class ConfigManager
 * @brief Фасад для управления конфигурацией системы
 *
 * @note Объединяет функциональность:
 * - Загрузка конфигурации из файла
 * - Обработка переменных окружения
 * - Валидация структуры
 * - Кеширование результатов
 *
 * @warning Не потокобезопасен при одновременном вызове методов modify
 */
class ConfigManager {
 public:
  static ConfigManager &instance();

  /**
   * @brief Инициализирует конфигурацию из файла
   * @param filename Путь к JSON-файлу конфигурации
   * @throw std::runtime_error При ошибках загрузки/валидации
   */
  void initialize(const std::string &filename);

  /**
   * @brief Считывает конфигурационный файл повторно.
   */
  void reload();

  /**
   * @brief Возвращает объединенную конфигурацию для окружения
   * @param env Идентификатор окружения (development/production)
   * @return nlohmann::json Кешированный результат слияния
   */
  nlohmann::json getMergedConfig(const std::string &env) const;

  std::string getGlobalComparisonList(const std::string &env) const;

  /**
   * @brief Применяет переопределения из CLI
   * @param overrides Маппинг ключ-значение для замены
   */
  void applyCliOverrides(
      const std::unordered_map<std::string, std::string> &overrides);

 private:
  ConfigManager() = default;
  ~ConfigManager() = default;

  void backupCurrentConfig();
  void restoreBackupConfig();
  bool validateConfigSafely(const nlohmann::json &config) const;

  ConfigLoader loader_;
  ConfigValidator validator_;
  EnvironmentProcessor envProcessor_;
  mutable ConfigCache cache_;
  nlohmann::json baseConfig_;
  nlohmann::json backupConfig_;
  std::string configFilePath_;
  mutable std::mutex configMutex_;
};