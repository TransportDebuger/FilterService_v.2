/**
@file config_reload_transaction.cpp
@brief Реализация транзакционной перезагрузки конфигурации.
@version 2.0.0
@date 2026-07-17
*/
#include "../include/config_reload_transaction.hpp"

#include <stdexcept>

namespace stc {

ConfigReloadTransaction::ConfigReloadTransaction(ConfigManager& configMgr,
                                                 std::shared_ptr<stc::logger::ILogger> logger)
    : configMgr_(configMgr), logger_(std::move(logger)) {}

ConfigReloadTransaction::~ConfigReloadTransaction() {
    if (active_) {
        try {
            rollback();
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error(std::string("ConfigReloadTransaction:: rollback failed: ") + e.what());
            }
        }
    }
}

void ConfigReloadTransaction::begin() {
    if (active_) {
        throw std::runtime_error("ConfigReloadTransaction: Transaction already active");
    }
    // Используем публичный геттер, который внутри себя корректно захватывает мьютекс ConfigManager
    backup_ = configMgr_.getCurrentConfig();
    active_ = true;
    if (logger_) logger_->Debug("ConfigReloadTransaction: backup created");
}

void ConfigReloadTransaction::commit() {
    if (!active_) {
        throw std::runtime_error("ConfigReloadTransaction: No active transaction");
    }
    active_ = false;
    backup_.clear(); // Освобождаем память, занимаемую резервной копией
    if (logger_) logger_->Debug("ConfigReloadTransaction: committed");
}

void ConfigReloadTransaction::rollback() {
    if (!active_) {
        throw std::runtime_error("ConfigReloadTransaction: No active transaction");
    }
    configMgr_.restoreFromBackup(backup_);
    active_ = false;
    backup_.clear();
    if (logger_) logger_->Info("ConfigReloadTransaction: rolled back");
}

void ConfigReloadTransaction::reload() {
    begin();
    try {
        configMgr_.reload();
        commit();
        if (logger_) logger_->Info("ConfigReloadTransaction: reload successful");
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Warning(std::string("ConfigReloadTransaction: reload failed, rolling back: ") + e.what());
        }
        rollback();
        throw;
    }
}

} // namespace stc