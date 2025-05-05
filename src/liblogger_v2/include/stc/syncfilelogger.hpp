#pragma once
#include "stc/basefilelogger.hpp"

namespace stc {

class SyncFileLogger : public BaseFileLogger {
public:
    static SyncFileLogger& instance();

protected:
    void writeToFile(const std::string& message) override;

private:
    SyncFileLogger() = default;
};

} // namespace stc