#include "../include/configmanager.hpp"

#include <iostream>
#include <stdexcept>

ConfigManager &ConfigManager::instance() {
  static ConfigManager instance;
  return instance;
}

void ConfigManager::initialize(const std::string &filename) {
  std::lock_guard<std::mutex> lock(configMutex_);

  configFilePath_ = filename;
  try {
    baseConfig_ = loader_.loadFromFile(filename);
    envProcessor_.process(baseConfig_);

    if (!validator_.validateRoot(baseConfig_)) {
      throw std::runtime_error("Invalid config structure");
    }

  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Config initialization failed: ") +
                             e.what());
  }
}

void ConfigManager::reload() {
  std::lock_guard<std::mutex> lock(configMutex_);

  if (configFilePath_.empty()) {
    throw std::runtime_error("No configuration file path available for reload");
  }

  try {
    // Создаем резервную копию текущей конфигурации
    backupCurrentConfig();

    // Пытаемся загрузить новую конфигурацию
    nlohmann::json newConfig = loader_.loadFromFile(configFilePath_);

    // Обработка переменных окружения
    envProcessor_.process(newConfig);

    // Валидация новой конфигурации
    if (!validateConfigSafely(newConfig)) {
      throw std::runtime_error("New configuration failed validation");
    }

    // Если валидация прошла успешно - применяем новую конфигурацию
    baseConfig_ = std::move(newConfig);

    // Очищаем кеш для перестроения с новой конфигурацией
    cache_.clearAll();

    std::cout << "Configuration reloaded successfully from: " << configFilePath_
              << std::endl;

  } catch (const std::exception &e) {
    // В случае ошибки восстанавливаем предыдущую конфигурацию
    restoreBackupConfig();
    std::cerr << "Failed to reload configuration: " << e.what() << std::endl;
    std::cerr << "Using previous configuration" << std::endl;
    throw std::runtime_error("Config reload failed: " + std::string(e.what()));
  }
}

nlohmann::json ConfigManager::getMergedConfig(const std::string &env) const {
  std::lock_guard<std::mutex> lock(configMutex_);

  if (auto cached = cache_.getCached(env); !cached.empty()) {
    return cached;
  }

  if (!baseConfig_.contains("environments") ||
      !baseConfig_["environments"].contains(env)) {
    throw std::runtime_error("Environment '" + env + "' not found");
  }

  nlohmann::json merged = baseConfig_["defaults"];
  merged.merge_patch(baseConfig_["environments"][env]);

  cache_.updateCache(env, merged);
  return merged;
}

std::string ConfigManager::getGlobalComparisonList(
    const std::string &env) const {
  auto config = getMergedConfig(env);
  return config.value("comparison_list", "./comparison_list.csv");
}
void ConfigManager::applyCliOverrides(
    const std::unordered_map<std::string, std::string> &overrides) {
  std::lock_guard<std::mutex> lock(configMutex_);

  // Применение CLI переопределений
  nlohmann::json overrideJson;
  for (const auto &[key, value] : overrides) {
    overrideJson[key] = value;
  }
  baseConfig_.merge_patch(overrideJson);

  // Полное обновление кеша для всех окружений
  if (baseConfig_.contains("environments") &&
      baseConfig_["environments"].is_object()) {
    for (const auto &[env, _] : baseConfig_["environments"].items()) {
      cache_.updateCache(env, nlohmann::json());
    }
  } else {
    // Обработка конфигов без секции environments
    cache_.updateCache("default", nlohmann::json());
  }
}

void ConfigManager::backupCurrentConfig() {
  if (!baseConfig_.empty()) {
    backupConfig_ = baseConfig_;
  }
}

void ConfigManager::restoreBackupConfig() {
  if (!backupConfig_.empty()) {
    baseConfig_ = backupConfig_;
    std::cout << "Configuration restored from backup" << std::endl;
  }
}

bool ConfigManager::validateConfigSafely(const nlohmann::json &config) const {
  try {
    return validator_.validateRoot(config);
  } catch (const std::exception &e) {
    std::cerr << "Validation error: " << e.what() << std::endl;
    return false;
  }
}
