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
        logQueue_.push(std::move(formattedMessage));
    }
    queueCV_.notify_one();
}

void AsyncFileLogger::processQueue() {
    auto now = []{ return std::chrono::steady_clock::now(); };
    lastFlushTime_ = now();

    while (running_ || !logQueue_.empty()) {
        // 1. Извлекаем пачку сообщений
        {
            std::unique_lock lock(queueMutex_);
            queueCV_.wait(lock, [this] {
                return !logQueue_.empty() || !running_;
            });

            while (!logQueue_.empty() && batchBuffer_.size() < maxBatchSize_) {
                batchBuffer_.push_back(std::move(logQueue_.front()));
                logQueue_.pop();
            }
        }

        // 2. Проверяем, пора ли делать flush (по размеру или по таймеру)
        bool needsFlush = batchBuffer_.size() >= maxBatchSize_ ||
                          (now() - lastFlushTime_) >= flushInterval_;

        // 3. Записываем и flush-аем, если нужно
        if (!batchBuffer_.empty() && needsFlush) {
            flushBatch();
        }
    }

    // 4. Записываем оставшиеся сообщения при завершении
    if (!batchBuffer_.empty()) {
        flushBatch(true);
    }
}

void AsyncFileLogger::flushBatch(bool force) {
    if (batchBuffer_.empty()) return;

    std::lock_guard fileLock(mutex_);
    try {
        for (const auto& msg : batchBuffer_) {
            if (mainLogFile_.is_open()) {
                mainLogFile_ << msg;
                warnedAboutFallback_ = false;
            } else if (fallbackLogFile_.is_open()) {
                if (!warnedAboutFallback_) {
                    std::cerr << "[LOGGER WARNING] Main log file unavailable, switching to fallback log file: "
                              << fallbackLogPath_ << std::endl;
                    warnedAboutFallback_ = true;
                }
                fallbackLogFile_ << msg;
            } else {
                std::cerr << "[LOGGER ERROR] No log file is open for writing! Attempting to reopen files..." << std::endl;
                reopenFiles();
                // Повторная попытка
                if (mainLogFile_.is_open()) {
                    mainLogFile_ << msg;
                    warnedAboutFallback_ = false;
                } else if (fallbackLogFile_.is_open()) {
                    if (!warnedAboutFallback_) {
                        std::cerr << "[LOGGER WARNING] Main log file unavailable after reopen, switching to fallback log file: "
                                  << fallbackLogPath_ << std::endl;
                        warnedAboutFallback_ = true;
                    }
                    fallbackLogFile_ << msg;
                } else {
                    std::cerr << "[LOGGER ERROR] Still no log file is open for writing after reopen!" << std::endl;
                }
            }
        }
        // Flush после пачки
        if (mainLogFile_.is_open()) mainLogFile_.flush();
        else if (fallbackLogFile_.is_open()) fallbackLogFile_.flush();
        batchBuffer_.clear();
        lastFlushTime_ = std::chrono::steady_clock::now();
    } catch (const std::exception& e) {
        std::cerr << "[LOGGER ERROR] Exception during async file write: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[LOGGER ERROR] Unknown exception during async file write." << std::endl;
    }
}

void AsyncFileLogger::setFlushInterval(std::chrono::milliseconds interval) {
    std::lock_guard lock(queueMutex_);
    flushInterval_ = interval;
}

void AsyncFileLogger::setMaxBatchSize(size_t size) {
    std::lock_guard lock(queueMutex_);
    maxBatchSize_ = size;
}

} // namespace stc