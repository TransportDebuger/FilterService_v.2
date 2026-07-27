/**
@file adapterfactory.cpp
@brief Реализация фабрики адаптеров файловых хранилищ.
@version 2.2.0
@date 2026-07-24
*/
#include "AdapterFactory.hpp"
#include <algorithm>
#include <stdexcept>
#include "FtpFileAdapter.hpp"
#include "LocalStorageAdapter.hpp"

namespace stc {

AdapterFactory::AdapterFactory() {
    registerBuiltinAdapters();
}

std::unique_ptr<IFileStorage> AdapterFactory::createAdapter(
    const SourceConfig& config, std::shared_ptr<stc::logger::ILogger> logger) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config.type.empty()) {
        throw std::invalid_argument("Storage type cannot be empty");
    }
    
    auto it = creators_.find(config.type);
    if (it == creators_.end()) {
        throw std::invalid_argument("Unsupported storage type: " + config.type);
    }
    
    try {
        auto adapter = it->second(config, logger);
        if (logger) {
            logger->Info("Created adapter for type: " + config.type + ", path: " + config.path);
        }
        return adapter;
    } catch (const std::exception& e) {
        if (logger) {
            logger->Error("Failed to create adapter for type " + config.type + ": " + e.what());
        }
        throw std::runtime_error("Adapter creation failed: " + std::string(e.what()));
    }
}

void AdapterFactory::registerAdapter(const std::string& type, CreatorFunction creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (type.empty()) {
        throw std::invalid_argument("Adapter type cannot be empty");
    }
    if (!creator) {
        throw std::invalid_argument("Creator function cannot be null");
    }
    creators_[type] = std::move(creator);
}

bool AdapterFactory::isSupported(const std::string& type) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return creators_.find(type) != creators_.end();
}

std::vector<std::string> AdapterFactory::getSupportedTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> types;
    types.reserve(creators_.size());
    for (const auto& [type, _] : creators_) {
        types.push_back(type);
    }
    std::sort(types.begin(), types.end());
    return types;
}

void AdapterFactory::registerBuiltinAdapters() {
    registerAdapter("local", [](const SourceConfig& config, std::shared_ptr<stc::logger::ILogger> logger) {
        return std::make_unique<LocalStorageAdapter>(config, std::move(logger));
    });
    
    registerAdapter("ftp", [](const SourceConfig& config, std::shared_ptr<stc::logger::ILogger> logger) {
        if (!config.hasRequiredParams({"username", "password"})) {
            throw std::invalid_argument("FTP source requires 'username' and 'password' parameters");
        }
        return std::make_unique<FtpFileAdapter>(config, std::move(logger));
    });
}

} // namespace stc