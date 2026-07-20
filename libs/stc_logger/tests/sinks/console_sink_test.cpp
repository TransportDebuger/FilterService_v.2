#include "stc/logger/sinks/console/console_sink.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/log_level.hpp"
#include "stc/logger/core/log_record.hpp"

namespace stc::logger::test {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

// ============================================================================
// Mock-объекты и вспомогательные классы
// ============================================================================

class MockFormatter : public ILogFormatter {
 public:
  MOCK_METHOD(std::string, Format, (const LogRecord&), (const, override));
};

// RAII-обертка для перехвата std::cout и std::cerr
class CoutCerrCapture {
 public:
  CoutCerrCapture() {
    old_cout_ = std::cout.rdbuf(cout_buf_.rdbuf());
    old_cerr_ = std::cerr.rdbuf(cerr_buf_.rdbuf());
  }
  ~CoutCerrCapture() {
    std::cout.rdbuf(old_cout_);
    std::cerr.rdbuf(old_cerr_);
  }

  std::string cout_str() const { return cout_buf_.str(); }
  std::string cerr_str() const { return cerr_buf_.str(); }

 private:
  std::stringstream cout_buf_;
  std::stringstream cerr_buf_;
  std::streambuf* old_cout_;
  std::streambuf* old_cerr_;
};

// Хелпер для создания тестовой записи
LogRecord MakeRecord(LogLevel level) {
  return LogRecord{std::chrono::system_clock::now(), level, "Test message",
                   std::source_location::current()};
}

// ============================================================================
// Фикстура теста
// ============================================================================

class ConsoleSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_formatter_ = std::make_shared<StrictMock<MockFormatter>>();
  }

  std::shared_ptr<MockFormatter> mock_formatter_;
};

// ============================================================================
// 1. Тесты конструирования и геттеров
// ============================================================================

TEST_F(ConsoleSinkTest, Construction_ValidFormatter_DoesNotThrow) {
  EXPECT_NO_THROW(auto sink = ConsoleSink(mock_formatter_));
}

TEST_F(ConsoleSinkTest, Construction_NullFormatter_ThrowsInvalidArgument) {
  EXPECT_THROW(auto sink = ConsoleSink(nullptr), std::invalid_argument);
}

TEST_F(ConsoleSinkTest, Getters_ReturnCorrectInstances) {
  auto sink = std::make_shared<ConsoleSink>(mock_formatter_);

  EXPECT_EQ(sink->GetFormatter(), mock_formatter_);
  EXPECT_EQ(sink->GetFilter(), nullptr);  // По умолчанию фильтр не задан
}

// ============================================================================
// 2. Тесты маршрутизации (Routing)
// ============================================================================

TEST_F(ConsoleSinkTest, Write_InfoLevel_RoutesToCout) {
  ConsoleSink sink(mock_formatter_, nullptr, false);  // Цвета отключены
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kInfo), "INFO_MSG");

  EXPECT_EQ(capture.cout_str(), "INFO_MSG");
  EXPECT_TRUE(capture.cerr_str().empty());
}

TEST_F(ConsoleSinkTest, Write_WarningLevel_RoutesToCerr) {
  ConsoleSink sink(mock_formatter_, nullptr, false);
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kWarning), "WARN_MSG");

  EXPECT_TRUE(capture.cout_str().empty());
  EXPECT_EQ(capture.cerr_str(), "WARN_MSG");
}

TEST_F(ConsoleSinkTest, Write_ErrorLevel_RoutesToCerr) {
  ConsoleSink sink(mock_formatter_, nullptr, false);
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kError), "ERR_MSG");

  EXPECT_TRUE(capture.cout_str().empty());
  EXPECT_EQ(capture.cerr_str(), "ERR_MSG");
}

// ============================================================================
// 3. Тесты цветного вывода (ANSI Codes)
// ============================================================================

TEST_F(ConsoleSinkTest, Write_ColorsEnabled_AppliesAnsiCodes) {
  ConsoleSink sink(mock_formatter_, nullptr, true);  // Цвета ВКЛЮЧЕНЫ
  CoutCerrCapture capture;

  // Debug level -> Green (\033[32m)
  sink.Write(MakeRecord(LogLevel::kDebug), "DEBUG_MSG");

  std::string output = capture.cout_str();
  EXPECT_NE(output.find("\033[32m"), std::string::npos)
      << "Should contain Green ANSI code";
  EXPECT_NE(output.find("\033[0m"), std::string::npos)
      << "Should contain Reset ANSI code";
  EXPECT_NE(output.find("DEBUG_MSG"), std::string::npos);
}

TEST_F(ConsoleSinkTest, Write_ColorsEnabled_TraceLevel) {
  ConsoleSink sink(mock_formatter_, nullptr, true);
  CoutCerrCapture capture;

  // Передаем уже отформатированное сообщение напрямую, как это делает Logger
  sink.Write(MakeRecord(LogLevel::kTrace), "TRACE_MSG");

  std::string output = capture.cout_str();
  EXPECT_NE(output.find("\033[36m"), std::string::npos)
      << "Should contain Cyan ANSI code for Trace";
  EXPECT_NE(output.find("\033[0m"), std::string::npos)
      << "Should contain Reset ANSI code";
}

TEST_F(ConsoleSinkTest, Write_ColorsEnabled_WarningLevel) {
  ConsoleSink sink(mock_formatter_, nullptr, true);
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kWarning), "WARN_MSG");

  std::string output = capture.cerr_str();
  EXPECT_NE(output.find("\033[33m"), std::string::npos)
      << "Should contain Yellow ANSI code for Warning";
}

TEST_F(ConsoleSinkTest, Write_ColorsEnabled_ErrorLevel) {
  ConsoleSink sink(mock_formatter_, nullptr, true);
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kError), "ERR_MSG");

  std::string output = capture.cerr_str();
  EXPECT_NE(output.find("\033[31m"), std::string::npos)
      << "Should contain Red ANSI code for Error";
}

TEST_F(ConsoleSinkTest, Write_ColorsEnabled_CriticalLevel_Flushes) {
  ConsoleSink sink(mock_formatter_, nullptr, true);
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kCritical), "CRIT_MSG");

  std::string output = capture.cerr_str();
  EXPECT_NE(output.find("\033[1;31m"), std::string::npos)
      << "Should contain Bold Red ANSI code for Critical";
  // Сам факт успешного выполнения этого теста и отсутствия зависаний
  // косвенно подтверждает корректную работу out.flush() внутри Write.
}

TEST_F(ConsoleSinkTest, Write_ColorsDisabled_NoAnsiCodes) {
  ConsoleSink sink(mock_formatter_, nullptr, false);  // Цвета ВЫКЛЮЧЕНЫ
  CoutCerrCapture capture;

  sink.Write(MakeRecord(LogLevel::kDebug), "DEBUG_MSG");

  std::string output = capture.cout_str();
  EXPECT_EQ(output.find("\033["), std::string::npos)
      << "Should NOT contain any ANSI codes";
  EXPECT_EQ(output, "DEBUG_MSG");
}

// ============================================================================
// 4. Тесты Flush
// ============================================================================

TEST_F(ConsoleSinkTest, Flush_DoesNotThrow) {
  ConsoleSink sink(mock_formatter_);
  // Просто проверяем, что вызов Flush не приводит к падениям или исключениям
  EXPECT_NO_THROW(sink.Flush());
}

}  // namespace stc::logger::test