#pragma once
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

class ConfigCache {
 public:
  /**
   * @brief Получает закешированную конфигурацию для указанного окружения
   * @param env Идентификатор окружения (например "production")
   * @return nlohmann::json Кешированная конфигурация или пустой объект
   */
  nlohmann::json getCached(const std::string& env) const;

  /**
   * @brief Обновляет кеш для указанного окружения
   * @param env Идентификатор окружения
   * @param config Новая конфигурация для кеширования
   */
  void updateCache(const std::string& env, const nlohmann::json& config);

 private:
  mutable std::mutex cacheMutex;  ///< Мьютекс для синхронизации доступа
  nlohmann::json cachedConfig;  ///< Кешированные данные конфигурации
};