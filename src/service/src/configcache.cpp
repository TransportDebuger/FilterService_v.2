/**
 * @file configcache.cpp
 * @author FilterService Development Team
 * @company FilterService Solutions Ltd.
 * @date June 2025
 * @brief Реализация кэша конфигураций
 *
 * @details
 * Содержит реализацию методов класса ConfigCache для кэширования
 * конфигураций различных окружений XML Filter Service. Обеспечивает
 * потокобезопасный доступ к кэшированным данным и их обновление.
 *
 * Ключевые особенности реализации:
 * - Синхронизация доступа с помощью std::mutex
 * - Валидация входных данных перед сохранением в кэш
 * - Оптимизированный доступ к кэшированным конфигурациям
 *
 * @version 1.0
 */
#include "../include/configcache.hpp"

#include <stdexcept>

nlohmann::json ConfigCache::getCached(const std::string &env) const {
  std::lock_guard<std::mutex> lock(cacheMutex_);

  if (!cachedConfig_.empty() && cachedConfig_.contains(env)) {
    return cachedConfig_[env];
  }
  return nlohmann::json();
}

void ConfigCache::updateCache(const std::string &env,
                              const nlohmann::json &config) {
  std::lock_guard<std::mutex> lock(cacheMutex_);

  if (config.is_null() || !config.is_object()) {
    throw std::invalid_argument("Invalid config for caching");
  }

  if (cachedConfig_.empty()) {
    cachedConfig_ = nlohmann::json::object();
  }
  cachedConfig_[env] = config;
}

void ConfigCache::clearAll() {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  cachedConfig_.clear();
}