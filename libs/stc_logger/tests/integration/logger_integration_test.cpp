#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "stc/logger/filters/composite_filter.hpp"
#include "stc/logger/filters/level_filter.hpp"
#include "stc/logger/filters/source_filter.hpp"
#include "stc/logger/formatters/json_formatter.hpp"
#include "stc/logger/formatters/text_formatter.hpp"
#include "stc/logger/formatters/xml_formatter.hpp"
#include "stc/logger/logger.hpp"
#include "stc/logger/sinks/console/console_sink.hpp"
#include "stc/logger/sinks/file/async_file_sink.hpp"
#include "stc/logger/sinks/file/file_sink.hpp"
#include "stc/logger/sinks/file/size_rotation_policy.hpp"

namespace fs = std::filesystem;
using namespace stc::logger;

// Вспомогательная функция для чтения всего файла в строку
std::string ReadFileToString(const fs::path& path) {
  std::ifstream ifs(path);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     (std::istreambuf_iterator<char>()));
}

// Фикстура теста для управления временной директорией
class LoggerIntegrationTest : public ::testing::Test {
 protected:
  fs::path test_dir_;

  void SetUp() override {
    test_dir_ =
        fs::temp_directory_path() / fs::path("stc_logger_test_") /
        fs::path(std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(test_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(test_dir_, ec);
  }
};

// ============================================================================
// Тест 1: Базовая маршрутизация, фильтрация и форматирование (Text)
// ============================================================================
TEST_F(LoggerIntegrationTest, BasicRoutingAndFiltering) {
  fs::path log_file = test_dir_ / "basic.log";
  auto formatter =
      std::make_shared<TextFormatter>("[%level] %msg\n", "%H:%M:%S");
  auto console_filter = std::make_shared<LevelFilter>(LogLevel::kInfo);
  auto file_filter = std::make_shared<LevelFilter>(LogLevel::kDebug);

  auto console_sink =
      std::make_shared<ConsoleSink>(formatter, console_filter, false);
  auto file_sink =
      std::make_shared<FileSink>(log_file.string(), formatter, file_filter);

  Logger logger("IntegrationTest");
  logger.AddSink(console_sink);
  logger.AddSink(file_sink);

  logger.Debug("Debug message");
  logger.Info("Info message");
  logger.Warning("Warning message");
  logger.Flush();

  std::string file_content = ReadFileToString(log_file);
  EXPECT_NE(file_content.find("[DEBUG] Debug message"), std::string::npos);
  EXPECT_NE(file_content.find("[INFO] Info message"), std::string::npos);
  EXPECT_NE(file_content.find("[WARNING] Warning message"), std::string::npos);
}

// ============================================================================
// Тест 2: JsonFormatter и корректность экранирования (NDJSON)
// ============================================================================
TEST_F(LoggerIntegrationTest, JsonFormatterEscapingAndFormat) {
  fs::path log_file = test_dir_ / "json_test.log";
  auto formatter = std::make_shared<JsonFormatter>();
  auto sink = std::make_shared<FileSink>(log_file.string(), formatter);

  Logger logger("JsonTest");
  logger.AddSink(sink);

  // Сообщение с кавычками, переносами строк и обратными слешами
  logger.Info("Message with \"quotes\", \nnewlines, and \\backslashes");
  // Сообщение с XML-подобными тегами (в JSON < и > не обязаны экранироваться,
  // но амперсанды и контрольные символы должны)
  logger.Warning("XML-like tags: <tag attr=\"val\"> & </tag>");
  logger.Flush();

  std::string content = ReadFileToString(log_file);

  // Проверка корректности экранирования
  EXPECT_NE(content.find("\\\"quotes\\\""), std::string::npos)
      << "Quotes must be escaped";
  EXPECT_NE(content.find("\\nnewlines"), std::string::npos)
      << "Newlines must be escaped";
  EXPECT_NE(content.find("\\\\backslashes"), std::string::npos)
      << "Backslashes must be escaped";

  // Проверка структуры JSON
  EXPECT_NE(content.find("\"level\":\"INFO\""), std::string::npos);
  EXPECT_NE(content.find("\"level\":\"WARNING\""), std::string::npos);
  EXPECT_NE(content.find("\"message\":\""), std::string::npos);

  // Проверка, что каждая строка заканчивается переносом (NDJSON)
  EXPECT_EQ(content.back(), '\n');
}

// ============================================================================
// Тест 3: XmlFormatter и корректность экранирования (NDXML)
// ============================================================================
TEST_F(LoggerIntegrationTest, XmlFormatterEscapingAndFormat) {
  fs::path log_file = test_dir_ / "xml_test.log";
  auto formatter = std::make_shared<XmlFormatter>();
  auto sink = std::make_shared<FileSink>(log_file.string(), formatter);

  Logger logger("XmlTest");
  logger.AddSink(sink);

  // Сообщение с символами, ломающими XML-разметку
  logger.Info("Test & <verify> \"escaping\" 'single'");
  logger.Flush();

  std::string content = ReadFileToString(log_file);

  // Проверка корректности XML-экранирования
  EXPECT_NE(content.find("&amp;"), std::string::npos)
      << "Ampersand must be escaped";
  EXPECT_NE(content.find("&lt;verify&gt;"), std::string::npos)
      << "Brackets must be escaped";
  EXPECT_NE(content.find("&quot;escaping&quot;"), std::string::npos)
      << "Double quotes must be escaped";
  EXPECT_NE(content.find("&apos;single&apos;"), std::string::npos)
      << "Single quotes must be escaped";

  // Проверка структуры NDXML
  EXPECT_NE(content.find("<log timestamp="), std::string::npos);
  EXPECT_NE(content.find("</log>"), std::string::npos);
  EXPECT_EQ(content.back(), '\n');
}

// ============================================================================
// Тест 4: AsyncFileSink (Пакетная запись и гарантированный Flush)
// ============================================================================
TEST_F(LoggerIntegrationTest, AsyncFileSinkBatchingAndFlush) {
  fs::path log_file = test_dir_ / "async_test.log";
  auto formatter = std::make_shared<TextFormatter>("%msg\n");

  // Настраиваем малый размер батча и короткий интервал для быстрого
  // срабатывания
  auto sink = std::make_shared<AsyncFileSink>(
      log_file.string(), formatter, nullptr, nullptr,
      1024,                          // max_batch_size = 1 KB
      std::chrono::milliseconds(50)  // flush_interval = 50 ms
  );

  Logger logger("AsyncTest");
  logger.AddSink(sink);

  constexpr int kThreads = 5;
  constexpr int kMsgsPerThread = 100;
  std::vector<std::thread> threads;

  // Многопоточная генерация логов
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&logger, i]() {
      for (int j = 0; j < kMsgsPerThread; ++j) {
        logger.Info("Async msg " + std::to_string(i) + "-" + std::to_string(j));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Критически важно: вызываем явный Flush, чтобы дождаться_drain_ очереди
  // и записи всех батчей на диск фоновым потоком.
  logger.Flush();

  // Проверяем, что ни одно сообщение не потеряно
  std::ifstream ifs(log_file);
  int line_count = std::count(std::istreambuf_iterator<char>(ifs),
                              std::istreambuf_iterator<char>(), '\n');

  EXPECT_EQ(line_count, kThreads * kMsgsPerThread)
      << "AsyncFileSink must guarantee delivery of all messages after Flush()";
}

// ============================================================================
// Тест 5: Механика ротации файлов по размеру (SizeRotationPolicy)
// ============================================================================
TEST_F(LoggerIntegrationTest, FileSizeRotation) {
  fs::path log_file = test_dir_ / "rotating.log";
  auto rotation_policy = std::make_shared<SizeRotationPolicy>(100, 2);
  auto formatter = std::make_shared<TextFormatter>("%msg\n");
  auto file_sink = std::make_shared<FileSink>(log_file.string(), formatter,
                                              nullptr, rotation_policy);

  Logger logger("RotationTest");
  logger.AddSink(file_sink);

  for (int i = 1; i <= 7; ++i) {
    logger.Info("Message " + std::to_string(i) +
                ": Lorem ipsum dolor sit amet");
  }
  logger.Flush();

  EXPECT_TRUE(fs::exists(log_file));

  int archive_count = 0;
  for (const auto& entry : fs::directory_iterator(test_dir_)) {
    if (entry.path().filename().string().find("rotating.log.") !=
        std::string::npos) {
      archive_count++;
    }
  }
  EXPECT_LE(archive_count, 2) << "Should not keep more than 2 archives";
}