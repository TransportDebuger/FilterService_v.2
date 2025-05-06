#include "stc/syncfilelogger.hpp"

#include <iostream>

namespace stc {

  SyncFileLogger& SyncFileLogger::instance() {
    static SyncFileLogger instance;
    return instance;
}

// Реализация writeToFile()
void SyncFileLogger::writeToFile(const std::string& message) {
    std::lock_guard lock(mutex_);
    static bool warnedAboutFallback = false;

    try {
        // Пытаемся записать в основной лог-файл
        if (mainLogFile_.is_open()) {
            mainLogFile_ << message;
            mainLogFile_.flush();
            // Если запись в основной файл успешна, сбрасываем флаг предупреждения
            warnedAboutFallback = false;
        }
        // Если основной не доступен, пробуем fallback
        else if (fallbackLogFile_.is_open()) {
            if (!warnedAboutFallback) {
                std::cerr << "[LOGGER WARNING] Main log file unavailable, switching to fallback log file: "
                          << fallbackLogPath_ << std::endl;
                warnedAboutFallback = true;
            }
            fallbackLogFile_ << message;
            fallbackLogFile_.flush();
        }
        // Если оба файла недоступны, пробуем переоткрыть и повторить попытку записи
        else {
            std::cerr << "[LOGGER ERROR] No log file is open for writing! Attempting to reopen files..." << std::endl;
            reopenFiles();

            // Повторная попытка записи
            if (mainLogFile_.is_open()) {
                mainLogFile_ << message;
                mainLogFile_.flush();
                warnedAboutFallback = false;
            } else if (fallbackLogFile_.is_open()) {
                if (!warnedAboutFallback) {
                    std::cerr << "[LOGGER WARNING] Main log file unavailable after reopen, switching to fallback log file: "
                              << fallbackLogPath_ << std::endl;
                    warnedAboutFallback = true;
                }
                fallbackLogFile_ << message;
                fallbackLogFile_.flush();
            } else {
                std::cerr << "[LOGGER ERROR] Still no log file is open for writing after reopen!" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[LOGGER ERROR] Exception during file write: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[LOGGER ERROR] Unknown exception during file write." << std::endl;
    }
}

} // namespace stc