#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include "stc/syncfilelogger.hpp"

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

class SyncFileLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mainLog_ = std::make_unique<TempFile>("main");
        fallbackLog_ = std::make_unique<TempFile>("fallback");
        logger_ = &stc::SyncFileLogger::instance();
        logger_->setMainLogPath(mainLog_->path());
        logger_->setFallbackLogPath(fallbackLog_->path());
        logger_->init(stc::LogLevel::LOG_INFO);
    }
    void TearDown() override {
        // Убедимся, что файлы закрыты и всё записано
        logger_->flush();
    }
    std::unique_ptr<TempFile> mainLog_;
    std::unique_ptr<TempFile> fallbackLog_;
    stc::SyncFileLogger* logger_;
};

// Проверяем, что сообщение записывается в основной файл
TEST_F(SyncFileLoggerTest, WritesToMainFile) {
    const std::string message = "Test message to main file";
    logger_->info(message);
    logger_->flush();

    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find(message), std::string::npos);
}

// Проверяем fallback при недоступности основного файла
TEST_F(SyncFileLoggerTest, FallbackOnMainFileError) {
    fs::remove(mainLog_->path());

    const std::string message = "Test message to fallback";
    logger_->info(message);
    logger_->flush();

    std::ifstream file(fallbackLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find(message), std::string::npos);
}

// Проверяем, что при недоступности обоих файлов логгер не падает
TEST_F(SyncFileLoggerTest, HandlesBothFilesUnavailable) {
    fs::remove(mainLog_->path());
    fs::remove(fallbackLog_->path());

    EXPECT_NO_THROW(logger_->info("Exception test"));
    logger_->flush();
}

// Проверяем, что сообщение не записывается, если уровень ниже текущего
TEST_F(SyncFileLoggerTest, SkipsLowerLevel) {
    logger_->setLogLevel(stc::LogLevel::LOG_WARNING);
    const std::string message = "This should not appear";
    logger_->info(message);
    logger_->flush();

    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content.find(message), std::string::npos);
}

// Проверяем, что сообщение выводится, если уровень равен или выше текущего
TEST_F(SyncFileLoggerTest, OutputsHigherOrEqualLevel) {
    logger_->setLogLevel(stc::LogLevel::LOG_WARNING);
    const std::string message = "This should appear";
    logger_->warning(message);
    logger_->flush();
    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find(message), std::string::npos);
    logger_->error(message);
    file = std::ifstream(mainLog_->path());
    content = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find(message), std::string::npos);
}

// Проверяем форматирование сообщения (timestamp, level)
TEST_F(SyncFileLoggerTest, FormatsMessage) {
    const std::string message = "Formatted message";
    logger_->info(message);
    logger_->flush();

    std::ifstream file(mainLog_->path());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("INFO"), std::string::npos);
    EXPECT_NE(content.find(message), std::string::npos);
}

// Проверяем, что логгер не падает при пустом сообщении
TEST_F(SyncFileLoggerTest, HandlesEmptyMessage) {
    EXPECT_NO_THROW(logger_->info(""));
    logger_->flush();
}