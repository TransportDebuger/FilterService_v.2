#include "../include/configcache.hpp"
#include <stdexcept>

nlohmann::json ConfigCache::getCached(const std::string& env) const {
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    if (!cachedConfig.empty() && cachedConfig.contains(env)) {
        return cachedConfig[env];
    }
    return nlohmann::json();
}

void ConfigCache::updateCache(const std::string& env, const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    if (config.is_null() || !config.is_object()) {
        throw std::invalid_argument("Invalid config for caching");
    }
    
    if (cachedConfig.empty()) {
        cachedConfig = nlohmann::json::object();
    }
    cachedConfig[env] = config;
}