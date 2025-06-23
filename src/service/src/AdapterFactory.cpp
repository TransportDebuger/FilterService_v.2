/**
 * @file AdapterFactory.cpp
 * @brief Реализация фабрики адаптеров файловых хранилищ
 */

#include "../include/AdapterFactory.hpp"

#include <algorithm>
#include <mutex>

#include "stc/compositelogger.hpp"

AdapterFactory::AdapterFactory() {
  registerBuiltinAdapters();
  stc::CompositeLogger::instance().info("AdapterFactory initialized");
}

AdapterFactory &AdapterFactory::instance() {
  static AdapterFactory instance;
  return instance;
}

std::unique_ptr<FileStorageInterface> AdapterFactory::createAdapter(
    const SourceConfig &config) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (config.type.empty()) {
    throw std::invalid_argument("Storage type cannot be empty");
  }

  auto it = creators_.find(config.type);
  if (it == creators_.end()) {
    throw std::invalid_argument("Unsupported storage type: " + config.type);
  }

  try {
    auto adapter = it->second(config);

    stc::CompositeLogger::instance().info(
        "Created adapter for type: " + config.type + ", path: " + config.path);

    return adapter;

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error(
        "Failed to create adapter for type " + config.type + ": " +
        std::string(e.what()));
    throw std::runtime_error("Adapter creation failed: " +
                             std::string(e.what()));
  }
}

void AdapterFactory::registerAdapter(const std::string &type,
                                     CreatorFunction creator) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (type.empty()) {
    throw std::invalid_argument("Adapter type cannot be empty");
  }

  if (!creator) {
    throw std::invalid_argument("Creator function cannot be null");
  }

  creators_[type] = creator;

  stc::CompositeLogger::instance().info("Registered adapter type: " + type);
}

bool AdapterFactory::isSupported(const std::string &type) const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return creators_.find(type) != creators_.end();
}

std::vector<std::string> AdapterFactory::getSupportedTypes() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<std::string> types;
  types.reserve(creators_.size());

  for (const auto &[type, creator] : creators_) {
    types.push_back(type);
  }

  std::sort(types.begin(), types.end());
  return types;
}

void AdapterFactory::registerBuiltinAdapters() {
  // Регистрация LocalStorageAdapter
  registerAdapter(
      "local",
      [](const SourceConfig &config) -> std::unique_ptr<FileStorageInterface> {
        return std::make_unique<LocalStorageAdapter>(config);
      });

  // Регистрация SmbFileAdapter
  registerAdapter(
      "smb",
      [](const SourceConfig &config) -> std::unique_ptr<FileStorageInterface> {
        // Валидация обязательных параметров для SMB
        std::vector<std::string> required_fields = {"username"};
        AdapterFactory::instance().validateRequiredFields(config,
                                                          required_fields);

        return std::make_unique<SmbFileAdapter>(config);
      });

  // Регистрация FtpFileAdapter
  registerAdapter(
      "ftp",
      [](const SourceConfig &config) -> std::unique_ptr<FileStorageInterface> {
        // Валидация обязательных параметров для FTP
        std::vector<std::string> required_fields = {"username", "password"};
        AdapterFactory::instance().validateRequiredFields(config,
                                                          required_fields);

        return std::make_unique<FtpFileAdapter>(config);
      });

  stc::CompositeLogger::instance().debug("Built-in adapters registered");
}

void AdapterFactory::validateRequiredFields(
    const SourceConfig &config,
    const std::vector<std::string> &required_fields) const {
  for (const auto &field : required_fields) {
    if (config.params.find(field) == config.params.end() ||
        config.params.at(field).empty()) {
      throw std::invalid_argument("Missing required field: " + field);
    }
  }
}