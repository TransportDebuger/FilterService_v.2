#include "stc/asyncfilelogger.hpp"

#include <iostream>

namespace stc {

AsyncFileLogger& AsyncFileLogger::instance() {
    static AsyncFileLogger instance;
    return instance;
}

AsyncFileLogger::AsyncFileLogger() {
    workerThread_ = std::thread(&AsyncFileLogger::processQueue, this); 
}

AsyncFileLogger::~AsyncFileLogger() {
    running_ = false;
    queueCV_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    flush(); // Гарантированная запись оставшихся сообщений
}

void AsyncFileLogger::writeToFile(const std::string& formattedMessage) {
    {
        std::lock_guard lock(queueMutex_);
        logQueue_.push(formattedMessage);
    }
    queueCV_.notify_one();
}

void AsyncFileLogger::processQueue() {
    while (running_ || !logQueue_.empty()) {
        std::unique_lock lock(queueMutex_);
        queueCV_.wait(lock, [this] {
            return !logQueue_.empty() || !running_;
        });

        while (!logQueue_.empty()) {
            auto msg = logQueue_.front();
            logQueue_.pop();
            lock.unlock();

            try {
                std::lock_guard fileLock(mutex_);
                if (mainLogFile_.is_open()) {
                    mainLogFile_ << msg;
                    mainLogFile_.flush();
                    warnedAboutFallback_ = false;
                } else if (fallbackLogFile_.is_open()) {
                    if (!warnedAboutFallback_) {
                        std::cerr << "[LOGGER WARNING] Main log file unavailable, switching to fallback log file: "
                                  << fallbackLogPath_ << std::endl;
                        warnedAboutFallback_ = true;
                    }
                    fallbackLogFile_ << msg;
                    fallbackLogFile_.flush();
                } else {
                    std::cerr << "[LOGGER ERROR] No log file is open for writing! Attempting to reopen files..." << std::endl;
                    reopenFiles();
                    // Повторная попытка
                    if (mainLogFile_.is_open()) {
                        mainLogFile_ << msg;
                        mainLogFile_.flush();
                        warnedAboutFallback_ = false;
                    } else if (fallbackLogFile_.is_open()) {
                        if (!warnedAboutFallback_) {
                            std::cerr << "[LOGGER WARNING] Main log file unavailable after reopen, switching to fallback log file: "
                                      << fallbackLogPath_ << std::endl;
                            warnedAboutFallback_ = true;
                        }
                        fallbackLogFile_ << msg;
                        fallbackLogFile_.flush();
                    } else {
                        std::cerr << "[LOGGER ERROR] Still no log file is open for writing after reopen!" << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[LOGGER ERROR] Exception during async file write: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[LOGGER ERROR] Unknown exception during async file write." << std::endl;
            }

            lock.lock();
        }
    }
}

} // namespace stc