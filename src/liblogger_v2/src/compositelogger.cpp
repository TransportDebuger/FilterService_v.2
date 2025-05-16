#include "stc/compositelogger.hpp"

namespace stc {

    CompositeLogger& CompositeLogger::instance() {
        static CompositeLogger instance;
        return instance;
    }

    void CompositeLogger::addLogger(const std::shared_ptr<ILogger>& logger) {
        loggers_.push_back(logger);
    }
    
    void CompositeLogger::init(const LogLevel level) {
        for (auto& logger : loggers_) {
            if (logger) logger->init(level);
        }
    }

    void CompositeLogger::setLogLevel(LogLevel level) {
        for (auto& logger : loggers_) {
            if (logger) logger->setLogLevel(level);
        }
    }

    void CompositeLogger::flush() {
        for (auto& logger : loggers_) {
            if (logger) logger->flush();
        }
    }

    void CompositeLogger::debug(const std::string& message) {
        for (auto& logger : loggers_) {
            if (logger) logger->debug(message);
        }
    }

    void CompositeLogger::info(const std::string& message) {
        for (auto& logger : loggers_) {
            if (logger) logger->info(message);
        }
    }

    void CompositeLogger::warning(const std::string& message) {
        for (auto& logger : loggers_) {
            if (logger) logger->warning(message);
        }
    }

    void CompositeLogger::error(const std::string& message) {
        for (auto& logger : loggers_) {
            if (logger) logger->error(message);
        }
    }

    void CompositeLogger::critical(const std::string& message) {
        for (auto& logger : loggers_) {
            if (logger) logger->critical(message);
        }
    }

    void CompositeLogger::log(LogLevel level, const std::string&) {}

    bool CompositeLogger::shouldSkipLog(LogLevel level) const {
        // CompositeLogger логирует всегда, делегирует фильтрацию вложенным логгерам
        return false;
    }
}