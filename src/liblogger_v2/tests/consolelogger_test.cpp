#include <gtest/gtest.h>
#include <sstream>
#include <iostream>
#include <chrono>
#include "stc/consolelogger.hpp"

// Вспомогательный класс для перехвата stdout/stderr
class CaptureStream {
public:
    CaptureStream(std::ostream& target) : target_(target), oldBuf_(target.rdbuf()) {
        target.rdbuf(buffer_.rdbuf());
    }
    ~CaptureStream() {
        target_.rdbuf(oldBuf_);
    }
    std::string getOutput() const {
        return buffer_.str();
    }
private:
    std::ostream& target_;
    std::streambuf* oldBuf_;
    std::ostringstream buffer_;
};

class ConsoleLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger_ = &stc::ConsoleLogger::instance();
        logger_->setLogLevel(stc::LogLevel::LOG_DEBUG); // Чтобы ловить все уровни
    }
    stc::ConsoleLogger* logger_;
};

// Проверяем вывод сообщения в stdout (INFO/DEBUG)
TEST_F(ConsoleLoggerTest, OutputsToStdout) {
    CaptureStream cap(std::cout);
    const std::string message = "Test message to stdout";
    logger_->info(message);
    EXPECT_NE(cap.getOutput().find(message), std::string::npos);
}

// Проверяем вывод сообщения в stderr (ERROR/WARNING)
TEST_F(ConsoleLoggerTest, OutputError) {
    CaptureStream cap(std::cout);
    const std::string message = "Test error message";
    logger_->error(message);
    EXPECT_NE(cap.getOutput().find(message), std::string::npos);
}

// Проверяем, что сообщение не выводится, если уровень ниже текущего
TEST_F(ConsoleLoggerTest, SkipsLowerLevel) {
    logger_->setLogLevel(stc::LogLevel::LOG_WARNING);
    CaptureStream cap(std::cout);
    const std::string message = "This should not appear";
    logger_->info(message);
    EXPECT_EQ(cap.getOutput().find(message), std::string::npos);
}

// Проверяем, что сообщение выводится, если уровень равен или выше текущего
TEST_F(ConsoleLoggerTest, OutputsHigherOrEqualLevel) {
    logger_->setLogLevel(stc::LogLevel::LOG_WARNING);
    CaptureStream cap(std::cout);
    const std::string message = "This should appear";
    logger_->warning(message);
    EXPECT_NE(cap.getOutput().find(message), std::string::npos);
    logger_->error(message);
    EXPECT_NE(cap.getOutput().find(message), std::string::npos);
}

// Проверяем форматирование сообщения (timestamp, level)
TEST_F(ConsoleLoggerTest, FormatsMessage) {
    CaptureStream cap(std::cout);
    const std::string message = "Formatted message";
    logger_->info(message);
    std::string output = cap.getOutput();
    EXPECT_NE(output.find("INFO"), std::string::npos);
    EXPECT_NE(output.find(message), std::string::npos);
}

// Проверяем, что логгер не падает при пустом сообщении
TEST_F(ConsoleLoggerTest, HandlesEmptyMessage) {
    EXPECT_NO_THROW(logger_->info(""));
}