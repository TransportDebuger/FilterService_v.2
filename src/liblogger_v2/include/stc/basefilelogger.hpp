#pragma once
#include "stc/ilogger.hpp"
#include "stc/irotatablelogger.hpp"
#include <fstream>
#include <filesystem>
#include <mutex>

namespace stc {

class BaseFileLogger : public ILogger, public IRotatableLogger {
public:

    void init(const LogLevel level) override;
    void setLogLevel(LogLevel level) override;
    void setRotationConfig(const RotationConfig& config) override;
    RotationConfig getRotationConfig() const override;
    void flush() override;
    void setMainLogPath(const std::string& path);
    void setFallbackLogPath(const std::string& path);
    std::string getMainLogPath() const;
    std::string getFallbackLogPath() const;
    // Реализация чисто виртуального метода log()
    void log(LogLevel level, const std::string& message) override;

protected:
    BaseFileLogger() = default;
    virtual ~BaseFileLogger() = default;
    virtual void writeToFile(const std::string& formattedMessage) = 0;

    // Общие методы
    void reopenFiles();
    void rotateIfNeeded(const std::string& message);

    // Общие поля
    mutable std::mutex mutex_;
    std::ofstream mainLogFile_;
    std::ofstream fallbackLogFile_;
    std::string mainLogPath_ = "app.log";
    std::string fallbackLogPath_ = "app_fallback.log";
    RotationConfig rotationConfig_;

private:
    std::atomic<LogLevel> currentLevel_ = LogLevel::LOG_INFO;
    bool shouldSkipLog(LogLevel level) const override;
};

}