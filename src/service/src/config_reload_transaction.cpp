#include "../include/config_reload_transaction.hpp"
#include "stc/compositelogger.hpp"

ConfigReloadTransaction::ConfigReloadTransaction(ConfigManager& configMgr)
    : configMgr_(configMgr) {}

ConfigReloadTransaction::~ConfigReloadTransaction() {
    if (active_) {
        try {
            rollback();
        } catch (const std::exception& e) {
            stc::CompositeLogger::instance().error(
                "ConfigReloadTransaction::~ConfigReloadTransaction: rollback failed: " + std::string(e.what()));
        }
    }
}

void ConfigReloadTransaction::begin() {
    if (active_) {
        throw std::runtime_error("Transaction already active");
    }
    std::lock_guard lock(configMgr_.configMutex_);
    backup_ = configMgr_.baseConfig_;
    active_ = true;
    stc::CompositeLogger::instance().debug("ConfigReloadTransaction: backup created");
}

void ConfigReloadTransaction::commit() {
    if (!active_) {
        throw std::runtime_error("No active transaction");
    }
    active_ = false;
    stc::CompositeLogger::instance().debug("ConfigReloadTransaction: committed");
}

void ConfigReloadTransaction::rollback() {
    if (!active_) {
        throw std::runtime_error("No active transaction");
    }
    {
        std::lock_guard lock(configMgr_.configMutex_);
        configMgr_.baseConfig_ = backup_;
        configMgr_.cache_.clearAll();
    }
    active_ = false;
    stc::CompositeLogger::instance().info("ConfigReloadTransaction: rolled back");
}

void ConfigReloadTransaction::reload() {
    begin();
    try {
        configMgr_.reload();
        commit();
        stc::CompositeLogger::instance().info("ConfigReloadTransaction: reload successful");
    } catch (const std::exception& e) {
        stc::CompositeLogger::instance().warning(
            "ConfigReloadTransaction: reload failed, rolling back: " + std::string(e.what()));
        rollback();
        throw;
    }
}