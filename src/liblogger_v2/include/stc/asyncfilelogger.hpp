#pragma once
#include "stc/basefilelogger.hpp"
#include <queue>
#include <thread>
#include <condition_variable>

namespace stc {

class AsyncFileLogger : public BaseFileLogger {
public:
    static AsyncFileLogger& instance();
    ~AsyncFileLogger();

protected:
    void writeToFile(const std::string& formattedMessage) override;

private:
    AsyncFileLogger();
    void processQueue();

    std::queue<std::string> logQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::thread workerThread_;
    std::atomic<bool> running_{true};
};

} // namespace stc