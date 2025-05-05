#include "stc/asyncfilelogger.hpp"

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
    std::lock_guard<std::mutex> lock(queueMutex_);
    logQueue_.push(formattedMessage);
    queueCV_.notify_one();
}

void AsyncFileLogger::processQueue() {
    while (running_ || !logQueue_.empty()) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCV_.wait(lock, [this]() { 
            return !logQueue_.empty() || !running_; 
        });

        while (!logQueue_.empty()) {
            auto msg = logQueue_.front();
            logQueue_.pop();
            lock.unlock();

            // Запись в файл через базовый класс
            {
                std::lock_guard<std::mutex> fileLock(mutex_);
                if (mainLogFile_.is_open()) {
                    mainLogFile_ << msg;
                } else if (fallbackLogFile_.is_open()) {
                    fallbackLogFile_ << msg;
                }
            }

            lock.lock();
        }
    }
}

} // namespace stc