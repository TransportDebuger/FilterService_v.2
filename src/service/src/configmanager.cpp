/**
@file configmanager.cpp
@brief Реализация методов ConfigManager
@version 1.1.0
@date 2026-07-17
*/
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
        if (!validateConfigSafely(baseConfig_)) {
            throw std::runtime_error("Invalid config structure or validation failed");
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(std::string("Config initialization failed: ") + e.what());
    }
}

void ConfigManager::reload() {
    std::lock_guard<std::mutex> lock(configMutex_);
    if (configFilePath_.empty()) {
        throw std::runtime_error("No configuration file path available for reload");
    }
    try {
        backupCurrentConfig();
        nlohmann::json newConfig = loader_.loadFromFile(configFilePath_);
        envProcessor_.process(newConfig);
        
        if (!validateConfigSafely(newConfig)) {
            throw std::runtime_error("New configuration failed validation");
        }
        
        baseConfig_ = std::move(newConfig);
        cache_.clearAll();
        std::cout << "Configuration reloaded successfully from: " << configFilePath_ << std::endl;
    } catch (const std::exception &e) {
        restoreBackupConfig();
        std::cerr << "Failed to reload configuration: " << e.what() << std::endl;
        throw std::runtime_error("Config reload failed: " + std::string(e.what()));
    }
}

nlohmann::json ConfigManager::getMergedConfig(const std::string &env) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    if (auto cached = cache_.getCached(env); !cached.empty()) {
        return cached;
    }
    if (!baseConfig_.contains("environments") || !baseConfig_["environments"].contains(env)) {
        throw std::runtime_error("Environment '" + env + "' not found");
    }
    nlohmann::json merged = baseConfig_.value("defaults", nlohmann::json::object());
    merged.merge_patch(baseConfig_["environments"][env]);
    cache_.updateCache(env, merged);
    return merged;
}

std::string ConfigManager::getGlobalComparisonList(const std::string &env) const {
    auto config = getMergedConfig(env);
    return config.value("comparison_list", "./comparison_list.csv");
}

std::vector<stc::LoggerSinkConfig> ConfigManager::getLoggingSinksConfig(const std::string &env) const {
    auto merged = getMergedConfig(env);
    std::vector<stc::LoggerSinkConfig> sinks;

    // Если секция logging отсутствует или не является массивом (например, устаревший объект из defaults),
    // возвращаем пустой вектор. LoggerFactory обработает этот случай созданием дефолтного консольного синка.
    if (!merged.contains("logging") || !merged["logging"].is_array()) {
        return sinks;
    }

    for (const auto& entry : merged["logging"]) {
        stc::LoggerSinkConfig cfg;
        cfg.type = entry.value("type", "console");
        cfg.level = entry.value("level", "info");
        cfg.formatter = entry.value("formatter", "text");
        cfg.file_path = entry.value("file", "");
        
        if (entry.contains("rotation") && entry["rotation"].is_object()) {
            const auto& rot = entry["rotation"];
            stc::RotationConfig rc;
            rc.type = rot.value("type", "size");
            
            // Конвертация мегабайт из конфигурации в байты для внутренних структур
            double max_size_mb = rot.value("max_size_mb", 10.0);
            rc.max_size_bytes = static_cast<std::uint64_t>(max_size_mb * 1024 * 1024);
            rc.max_archives = rot.value("max_archives", 5);
            
            rc.interval_sec = std::chrono::seconds(rot.value("interval_sec", 86400));
            rc.time_format = rot.value("time_format", "%Y%m%d_%H%M%S");
            
            cfg.rotation = rc;
        }
        sinks.push_back(std::move(cfg));
    }
    return sinks;
}

void ConfigManager::applyCliOverrides(const std::unordered_map<std::string, std::string> &overrides) {
    std::lock_guard<std::mutex> lock(configMutex_);
    nlohmann::json overrideJson;
    for (const auto &pair : overrides) {
        overrideJson[pair.first] = pair.second;
    }
    baseConfig_.merge_patch(overrideJson);
    if (baseConfig_.contains("environments") && baseConfig_["environments"].is_object()) {
        for (const auto &[env, _] : baseConfig_["environments"].items()) {
            cache_.updateCache(env, nlohmann::json());
        }
    } else {
        cache_.updateCache("default", nlohmann::json());
    }
}

const nlohmann::json& ConfigManager::getCurrentConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return baseConfig_;
}

void ConfigManager::restoreFromBackup(const nlohmann::json& backup) {
    std::lock_guard<std::mutex> lock(configMutex_);
    baseConfig_ = backup;
    cache_.clearAll();
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
        if (!validator_.validateRoot(config)) return false;

        // Валидация массивов sources и logging в корне конфигурации
        if (config.contains("sources") && config["sources"].is_array()) {
            if (!validator_.validateSources(config["sources"])) return false;
        }
        if (config.contains("logging") && config["logging"].is_array()) {
            if (!validator_.validateLogging(config["logging"])) return false;
        }

        // Глубокая валидация специфичных параметров для каждого окружения
        if (config.contains("environments") && config["environments"].is_object()) {
            for (const auto& [env, env_config] : config["environments"].items()) {
                if (env_config.is_object()) {
                    if (env_config.contains("sources") && env_config["sources"].is_array()) {
                        if (!validator_.validateSources(env_config["sources"])) return false;
                    }
                    if (env_config.contains("logging") && env_config["logging"].is_array()) {
                        if (!validator_.validateLogging(env_config["logging"])) return false;
                    }
                }
            }
        }
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Validation error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<stc::SourceConfig> ConfigManager::getSourcesConfig(const std::string &env) const {
    std::lock_guard<std::mutex> lock(configMutex_);

    if (!baseConfig_.contains("environments") || !baseConfig_["environments"].contains(env)) {
        throw std::runtime_error("Environment '" + env + "' not found");
    }

    const auto& env_config = baseConfig_["environments"][env];
    if (!env_config.contains("sources") || !env_config["sources"].is_array()) {
        throw std::runtime_error("Sources array not found or invalid for environment: " + env);
    }

    // Извлекаем глобальные дефолты для источников. 
    // В config.json defaults.sources является объектом, а не массивом.
    nlohmann::json default_sources = baseConfig_.value("defaults", nlohmann::json::object())
                                         .value("sources", nlohmann::json::object());

    std::vector<stc::SourceConfig> result;
    result.reserve(env_config["sources"].size());

    for (const auto& src_json : env_config["sources"]) {
        // Корректный мердж: берем дефолтный объект и накладываем поверх него 
        // конкретный источник из окружения. Это предотвращает потерю дефолтов,
        // которая произошла бы при прямом вызове getMergedConfig (из-за замены object на array).
        nlohmann::json merged_src = default_sources;
        merged_src.merge_patch(src_json);
        
        // Делегируем строгую десериализацию, маппинг monitoring_strategy и валидацию 
        // структуре SourceConfig. Любая ошибка пробросится наружу.
        result.push_back(stc::SourceConfig::fromJson(merged_src));
    }

    return result;
}