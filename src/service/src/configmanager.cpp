#include "../include/configmanager.hpp"
#include <stdexcept>

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::initialize(const std::string& filename) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    try {
        baseConfig_ = loader_.loadFromFile(filename);
        envProcessor_.process(baseConfig_);
        
        if(!validator_.validateRoot(baseConfig_)) {
            throw std::runtime_error("Invalid config structure");
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Config initialization failed: ") + e.what());
    }
}

nlohmann::json ConfigManager::getMergedConfig(const std::string& env) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    if (auto cached = cache_.getCached(env); !cached.empty()) {
        return cached;
    }

    if (!baseConfig_.contains("environments") || !baseConfig_["environments"].contains(env)) {
        throw std::runtime_error("Environment '" + env + "' not found");
    }

    nlohmann::json merged = baseConfig_["defaults"];
    merged.merge_patch(baseConfig_["environments"][env]);
    
    cache_.updateCache(env, merged);
    return merged;
}

void ConfigManager::applyCliOverrides(const std::unordered_map<std::string, std::string>& overrides) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // Применение CLI переопределений
    nlohmann::json overrideJson;
    for (const auto& [key, value] : overrides) {
        overrideJson[key] = value;
    }
    baseConfig_.merge_patch(overrideJson);

    // Полное обновление кеша для всех окружений
    if (baseConfig_.contains("environments") && baseConfig_["environments"].is_object()) {
        for (const auto& [env, _] : baseConfig_["environments"].items()) {
            cache_.updateCache(env, nlohmann::json());
        }
    } else {
        // Обработка конфигов без секции environments
        cache_.updateCache("default", nlohmann::json());
    }
}