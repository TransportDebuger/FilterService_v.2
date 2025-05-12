#pragma once

#include "stc/ilogger.hpp"
#include <vector>
#include <memory>
#include <initializer_list>

namespace stc {

class CompositeLogger : public ILogger {
public:
    CompositeLogger() = default;
    CompositeLogger(std::initializer_list<std::shared_ptr<ILogger>> loggers)
        : loggers_(loggers) {}

    void addLogger(const std::shared_ptr<ILogger>& logger);

    void init(const LogLevel level) override;

    void setLogLevel(LogLevel level) override;

    void flush() override;

    void debug(const std::string& message) override;
    void info(const std::string& message) override;
  void warning(const std::string& message) override;
  void error(const std::string& message) override;
  void critical(const std::string& message) override;

protected:
    bool shouldSkipLog(LogLevel level) const override;

private:
    std::vector<std::shared_ptr<ILogger>> loggers_;
};

} // namespace stc