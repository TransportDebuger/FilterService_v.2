#pragma once

#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

    class ConfigManager {
        public:
            static ConfigManager& instance();

            void loadFromFile(const std::string& filename);

            // Перезагрузка конфигурации (может вызываться в runtime)
            void reload();

            // Проверка валидности конфигурации
            bool isValid() const;

            // Получение конфига окружения (копия для безопасности)
            nlohmann::json getEnvironmentConfig(const std::string& env) const;

            // Получение конфигурации логгера для окружения (копия)
            nlohmann::json getLoggingConfig(const std::string& env) const;
            bool validate(const nlohmann::json& config) const;

        private:
            ConfigManager() = default;
            ~ConfigManager() = default;

            ConfigManager(const ConfigManager&) = delete;
            ConfigManager& operator=(const ConfigManager&) = delete;

            mutable std::mutex mutex_;
            nlohmann::json config_;
            bool valid_ = false;
            std::string currentConfigFile_;
    };