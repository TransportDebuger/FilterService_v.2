#include "stc/syncfilelogger.hpp"

namespace stc {

  SyncFileLogger& SyncFileLogger::instance() {
    static SyncFileLogger instance;
    return instance;
}

// Реализация writeToFile()
void SyncFileLogger::writeToFile(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mainLogFile_.is_open()) {
        mainLogFile_ << message;
        mainLogFile_.flush();
    } else if (fallbackLogFile_.is_open()) {
        fallbackLogFile_ << message;
        fallbackLogFile_.flush();
    }
}

} // namespace stc