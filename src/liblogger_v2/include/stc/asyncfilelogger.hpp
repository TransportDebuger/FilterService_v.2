#pragma once

#include "stc/basefilelogger.hpp"

#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>

namespace stc {

class AsyncFileLogger : public BaseFileLogger {
public:
    static AsyncFileLogger& instance();

    void setFlushInterval(std::chrono::milliseconds interval);
    void setMaxBatchSize(size_t size);
protected:
    void writeToFile(const std::string& formattedMessage) override;
    void flushBatch(bool force = false);

private:
   AsyncFileLogger();
    ~AsyncFileLogger();
    void processQueue();

    std::vector<std::string> batchBuffer_;
    std::chrono::steady_clock::time_point lastFlushTime_;
    std::chrono::milliseconds flushInterval_{100}; // по умолчанию 100 мс
    size_t maxBatchSize_{100}; // по умолчанию 100 сообщений в пачке
    std::queue<std::string> logQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::thread workerThread_;
    std::atomic<bool> running_{true};
    bool warnedAboutFallback_ = false;
};

} // namespace stc