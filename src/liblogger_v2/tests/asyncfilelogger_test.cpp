#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "stc/asyncfilelogger.hpp"

namespace fs = std::filesystem;

// Вспомогательный класс для временных файлов
class TempFile {
public:
    TempFile(const std::string& prefix = "test") {
        path_ = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".log");
    }
    ~TempFile() {
        if (fs::exists(path_)) fs::remove(path_);
    }
    const std::string& path() const { return path_; }
private:
    std::string path_;
};

class AsyncFileLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mainLog_ = std::make_unique<TempFile>("main");
        fallbackLog_ = std::make_unique<TempFile>("fallback");
        logger_ = &stc::AsyncFileLogger::instance();
        logger_->setMainLogPath(mainLog_->path());
        logger_->setFallbackLogPath(fallbackLog_->path());
        logger_->init(stc::LogLevel::LOG_INFO);
    }
    void TearDown() override {
        // Ожидаем завершения всех сообщений
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        logger_->flush();
    }
    std::unique_ptr<TempFile> mainLog_;
    std::unique_ptr<TempFile> fallbackLog_;
    stc::AsyncFileLogger* logger_;
};

// Проверяем, что сообщение записывается в основной файл
TEST_F(AsyncFileLoggerTest, WritesToMainFile) {
    const std::string message = "Test message to main file";
    logger_->log(stc::LogLevel::LOG_INFO, message);
    logger_->flush(); // Гарантируем запись

    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find(message), std::string::npos);
}

// Проверяем fallback при недоступности основного файла
TEST_F(AsyncFileLoggerTest, FallbackOnMainFileError) {
    // Делаем основной файл недоступным
    fs::remove(mainLog_->path());

    const std::string message = "Test message to fallback";
    logger_->log(stc::LogLevel::LOG_INFO, message);
    logger_->flush();

    std::ifstream file(fallbackLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find(message), std::string::npos);
}

// Проверяем пакетную запись и flush по размеру пачки
TEST_F(AsyncFileLoggerTest, BatchWriteAndFlushOnSize) {
    logger_->setMaxBatchSize(5);
    logger_->setFlushInterval(std::chrono::seconds(10)); // Выключим flush по таймеру для чистоты теста

    for (int i = 0; i < 10; ++i) {
        logger_->log(stc::LogLevel::LOG_INFO, "Message " + std::to_string(i));
    }
    logger_->flush();

    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("Message 0"), std::string::npos);
    EXPECT_NE(content.find("Message 9"), std::string::npos);
}

// Проверяем flush по таймеру
TEST_F(AsyncFileLoggerTest, FlushOnTimer) {
    logger_->setMaxBatchSize(100); // Выключим flush по размеру пачки
    logger_->setFlushInterval(std::chrono::milliseconds(50));

    logger_->log(stc::LogLevel::LOG_INFO, "Timer test message");
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ждём flush

    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("Timer test message"), std::string::npos);
}

// Проверяем обработку исключений
TEST_F(AsyncFileLoggerTest, ExceptionHandling) {
    // Делаем оба файла недоступными
    fs::remove(mainLog_->path());
    fs::permissions(mainLog_->path(), fs::perms::none);
    fs::remove(fallbackLog_->path());
    fs::permissions(fallbackLog_->path(), fs::perms::none);

    EXPECT_NO_THROW(logger_->log(stc::LogLevel::LOG_INFO, "Exception test"));
    logger_->flush();
}

// Проверяем graceful shutdown
TEST_F(AsyncFileLoggerTest, GracefulShutdown) {
    for (int i = 0; i < 10; ++i) {
        logger_->log(stc::LogLevel::LOG_INFO, "Shutdown test " + std::to_string(i));
    }
    // Деструктор AsyncFileLoggerTest вызовет flush и дождётся завершения всех сообщений
}