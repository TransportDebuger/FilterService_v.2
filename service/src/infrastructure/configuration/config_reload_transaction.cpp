/**
@file config_reload_transaction.cpp
@brief Реализация транзакционной перезагрузки конфигурации.
@version 3.0.0
@date 2026-07-24
*/
#include "config_reload_transaction.hpp"

#include <stdexcept>

namespace stc {

ConfigReloadTransaction::ConfigReloadTransaction(
    ConfigurationService& service, std::shared_ptr<stc::logger::ILogger> logger)
    : service_(service), logger_(std::move(logger)) {}

ConfigReloadTransaction::~ConfigReloadTransaction() {
  if (active_) {
    try {
      service_.RestoreState();
      if (logger_)
        logger_->Warning(
            "ConfigReloadTransaction: auto-rolled back on destruction");
    } catch (...) {
    }
  }
}

ApplicationConfiguration ConfigReloadTransaction::reload() {
  service_.BackupState();
  active_ = true;

  try {
    ApplicationConfiguration new_config = service_.ReloadConfiguration();
    active_ = false;  // Фиксация транзакции
    if (logger_) logger_->Info("ConfigReloadTransaction: reload successful");
    return new_config;
  } catch (const std::exception& e) {
    service_.RestoreState();
    active_ = false;
    if (logger_)
      logger_->Error("ConfigReloadTransaction: reload failed, rolled back: " +
                     std::string(e.what()));
    throw;
  }
}

}  // namespace stc