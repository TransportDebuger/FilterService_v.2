#include "../include/configmanager.hpp"

#include <fstream>

    ConfigManager& ConfigManager::instance() {
        static ConfigManager instance;
        return instance;
    }

    void ConfigManager::loadFromFile(const std::string& filename) {
        std::lock_guard lock(mutex_);
    
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + filename);
        }
    
        nlohmann::json newConfig;
        file >> newConfig;
    
        if (!validate(newConfig)) {
            throw std::runtime_error("Config validation failed");
        }
    
        config_ = std::move(newConfig);
        valid_ = true;
        currentConfigFile_ = filename;
    }

    void ConfigManager::reload() {
        std::lock_guard lock(mutex_);
    
        if (currentConfigFile_.empty()) {
            throw std::runtime_error("No config file loaded yet");
        }
    
        std::ifstream file(currentConfigFile_);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file for reload: " + currentConfigFile_);
        }
    
        nlohmann::json newConfig;
        file >> newConfig;
    
        if (!validate(newConfig)) {
            throw std::runtime_error("Config validation failed on reload");
        }
    
        config_ = std::move(newConfig);
        valid_ = true;
    }

    bool ConfigManager::isValid() const {
        std::lock_guard lock(mutex_);
        return valid_;
    }

    nlohmann::json ConfigManager::getEnvironmentConfig(const std::string& env) const {
        std::lock_guard lock(mutex_);
        if (!valid_ || !config_.contains("environments") || !config_["environments"].contains(env)) {
            throw std::runtime_error("Environment config not found: " + env);
        }
        return config_["environments"][env];
    }
    
    nlohmann::json ConfigManager::getLoggingConfig(const std::string& env) const {
        auto envConfig = getEnvironmentConfig(env);
        if (!envConfig.contains("logging")) {
            throw std::runtime_error("Logging config not found for environment: " + env);
        }
        return envConfig["logging"];
    }
    
    bool ConfigManager::validate(const nlohmann::json& config) const {
        // Простейшая валидация структуры
        if (!config.is_object()) return false;
        if (!config.contains("environments") || !config["environments"].is_object()) return false;
    
        // TO-DO: Можно добавить более глубокую проверку, например:
        // - Проверить, что в каждом окружении есть секция logging
        // - Проверить типы полей, наличие обязательных параметров и т.п.
    
        return true;
    }