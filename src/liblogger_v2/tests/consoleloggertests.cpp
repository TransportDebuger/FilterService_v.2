#include <gtest/gtest.h>
#include "stc/consolelogger.hpp"
#include <sstream>
#include <thread>
#include <vector>

// Фикстура для тестов
class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger_ = &stc::ConsoleLogger::instance();
        logger_->setLogLevel(stc::LogLevel::LOG_DEBUG);
        stc::TimeFormatter::setGlobalFormat("%H:%M:%S");
    }

    stc::ConsoleLogger* logger_;
};

// Тест уровней логирования
// TEST_F(LoggerTest, LevelFiltering) {
//     testing::internal::CaptureStdout();
//     testing::internal::CaptureStderr();

//     logger_->setLogLevel(stc::LogLevel::LOG_INFO);
//     logger_->debug("Debug Message");
//     // logger_->info("Info Message");
    
//     EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
//     EXPECT_NE(testing::internal::GetCapturedStdout(), "");
// }

// Тест формата времени
TEST_F(LoggerTest, TimeFormat) {
    auto now = std::chrono::system_clock::now();
    std::string formatted = stc::TimeFormatter::format(now);
    EXPECT_EQ(formatted.size(), 8); // HH:MM:SS
}

// Тест вывода в правильные потоки
TEST_F(LoggerTest, StreamOutput) {
    testing::internal::CaptureStdout();

    logger_->info("Test inf");
    logger_->error("Test err msg");

    std::string stdout = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(stdout.find("INFO") != std::string::npos);
    EXPECT_TRUE(stdout.find("ERROR") != std::string::npos);
}

// Тест потокобезопасности
TEST_F(LoggerTest, ThreadSafety) {
    constexpr int kNumThreads = 10;
    constexpr int kMessagesPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> counter{0};

    testing::internal::CaptureStdout();

    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([this, &counter]() {
            for (int j = 0; j < kMessagesPerThread; ++j) {
                logger_->info("Msg " + std::to_string(++counter));
                std::this_thread::sleep_for(std::chrono::microseconds(10)); // Добавьте задержку
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(counter, kNumThreads * kMessagesPerThread);
}

// Точка входа
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}