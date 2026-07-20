#include <gtest/gtest.h>

#include <chrono>
#include <source_location>
#include <string>

#include "stc/logger/core/log_level.hpp"
#include "stc/logger/core/log_record.hpp"
#include "stc/logger/formatters/json_formatter.hpp"
#include "stc/logger/formatters/text_formatter.hpp"
#include "stc/logger/formatters/xml_formatter.hpp"

namespace stc::logger::test {

// ============================================================================
// Helper для создания LogRecord с захватом source_location из места вызова
// ============================================================================
LogRecord MakeRecord(
    LogLevel level, std::string msg,
    std::source_location loc = std::source_location::current()) {
  return LogRecord{std::chrono::system_clock::now(), level, std::move(msg),
                   loc};
}

// ============================================================================
// 1. Тесты TextFormatter
// ============================================================================

TEST(FormattersTest, TextFormatter_AllPlaceholders) {
  TextFormatter fmt("[%level] %msg | %file:%line in %func | %time", "%H:%M:%S");
  auto record = MakeRecord(LogLevel::kWarning, "Test message");

  std::string result = fmt.Format(record);

  EXPECT_NE(result.find("[WARNING]"), std::string::npos);
  EXPECT_NE(result.find("Test message"), std::string::npos);
  EXPECT_NE(result.find("formatters_test.cpp"), std::string::npos);
  // Проверяем, что время отформатировалось (должны быть двоеточия от %H:%M:%S)
  EXPECT_NE(result.find(":"), std::string::npos);
}

TEST(FormattersTest, TextFormatter_UnknownPlaceholder) {
  TextFormatter fmt("%msg %unknown %%");
  auto record = MakeRecord(LogLevel::kInfo, "Data");

  std::string result = fmt.Format(record);

  // Неизвестные плейсхолдеры должны оставаться в строке без изменений
  EXPECT_EQ(result, "Data %unknown %%");
}

TEST(FormattersTest, TextFormatter_EmptyPattern) {
  TextFormatter fmt("");
  auto record = MakeRecord(LogLevel::kError, "Ignored");

  std::string result = fmt.Format(record);
  EXPECT_TRUE(result.empty());
}

TEST(FormattersTest, TextFormatter_LevelStrings_AllLevels) {
  // Используем шаблон, состоящий только из плейсхолдера уровня,
  // чтобы результат форматирования был равен строго строковому представлению.
  TextFormatter fmt("%level");

  // Проверка TRACE
  auto record_trace = MakeRecord(LogLevel::kTrace, "msg");
  EXPECT_EQ(fmt.Format(record_trace), "TRACE");

  // Проверка ERROR
  auto record_error = MakeRecord(LogLevel::kError, "msg");
  EXPECT_EQ(fmt.Format(record_error), "ERROR");

  // Проверка CRITICAL
  auto record_critical = MakeRecord(LogLevel::kCritical, "msg");
  EXPECT_EQ(fmt.Format(record_critical), "CRITICAL");

  // Проверка UNKNOWN (ветка default)
  // Форсируем невалидное значение enum через static_cast
  auto record_unknown = MakeRecord(static_cast<LogLevel>(99), "msg");
  EXPECT_EQ(fmt.Format(record_unknown), "UNKNOWN");
}

// ============================================================================
// 2. Тесты JsonFormatter
// ============================================================================

TEST(FormattersTest, JsonFormatter_BasicStructure) {
  JsonFormatter fmt;
  auto record = MakeRecord(LogLevel::kError, "Error occurred");

  std::string result = fmt.Format(record);

  // Проверка базовой структуры NDJSON
  EXPECT_EQ(result.front(), '{');
  EXPECT_EQ(result.back(), '\n');
  EXPECT_NE(result.find("\"level\":\"ERROR\""), std::string::npos);
  EXPECT_NE(result.find("\"message\":\"Error occurred\""), std::string::npos);
  EXPECT_NE(result.find("\"file\":\""), std::string::npos);
}

TEST(FormattersTest, JsonFormatter_EscapeSpecialChars) {
  JsonFormatter fmt;
  // Сообщение с кавычками, обратными слешами, переносами строк и табуляцией
  auto record = MakeRecord(LogLevel::kInfo, "Line1\nLine2\t\"Quoted\"\\Slash");

  std::string result = fmt.Format(record);

  EXPECT_NE(result.find("\\n"), std::string::npos) << "Newline must be escaped";
  EXPECT_NE(result.find("\\t"), std::string::npos) << "Tab must be escaped";
  EXPECT_NE(result.find("\\\"Quoted\\\""), std::string::npos)
      << "Quotes must be escaped";
  EXPECT_NE(result.find("\\\\Slash"), std::string::npos)
      << "Backslash must be escaped";
}

TEST(FormattersTest, JsonFormatter_EscapeControlChars) {
  JsonFormatter fmt;
  // Сообщение с управляющими символами ASCII < 0x20
  std::string msg = "Control\x01\x02\x1F";
  auto record = MakeRecord(LogLevel::kDebug, msg);

  std::string result = fmt.Format(record);

  EXPECT_NE(result.find("\\u0001"), std::string::npos);
  EXPECT_NE(result.find("\\u0002"), std::string::npos);
  EXPECT_NE(result.find("\\u001f"), std::string::npos);
}

TEST(FormattersTest, JsonFormatter_EscapeControlChars_BFR) {
  JsonFormatter fmt;
  // Сообщение с backspace, form feed и carriage return
  auto record = MakeRecord(LogLevel::kInfo, "BS:\b FF:\f CR:\r");

  std::string result = fmt.Format(record);

  EXPECT_NE(result.find("\\b"), std::string::npos)
      << "Backspace must be escaped";
  EXPECT_NE(result.find("\\f"), std::string::npos)
      << "Form feed must be escaped";
  EXPECT_NE(result.find("\\r"), std::string::npos)
      << "Carriage return must be escaped";
}

TEST(FormattersTest, JsonFormatter_TimeFormat_MillisecondsPadding) {
  JsonFormatter fmt;

  // Создаем время, где миллисекунды равны 15 (т.е. < 100 и >= 10)
  // Это покроет ветку else if (ms.count() < 100) { result.append("0"); }
  auto tp_15ms =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1015));
  LogRecord record_15{tp_15ms, LogLevel::kInfo, "msg",
                      std::source_location::current()};
  std::string result_15 = fmt.Format(record_15);
  EXPECT_NE(result_15.find(".015"), std::string::npos)
      << "Milliseconds should be padded to 015";

  // Создаем время, где миллисекунды равны 5 (т.е. < 10)
  // Это покроет ветку if (ms.count() < 10) { result.append("00"); }
  auto tp_5ms =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1005));
  LogRecord record_5{tp_5ms, LogLevel::kInfo, "msg",
                     std::source_location::current()};
  std::string result_5 = fmt.Format(record_5);
  EXPECT_NE(result_5.find(".005"), std::string::npos)
      << "Milliseconds should be padded to 005";
}

TEST(FormattersTest, JsonFormatter_LevelStrings_TraceCriticalUnknown) {
  JsonFormatter fmt;

  // Проверка TRACE
  auto record_trace = MakeRecord(LogLevel::kTrace, "trace");
  std::string res_trace = fmt.Format(record_trace);
  EXPECT_NE(res_trace.find("\"level\":\"TRACE\""), std::string::npos);

  // Проверка CRITICAL
  auto record_critical = MakeRecord(LogLevel::kCritical, "critical");
  std::string res_critical = fmt.Format(record_critical);
  EXPECT_NE(res_critical.find("\"level\":\"CRITICAL\""), std::string::npos);

  // Проверка UNKNOWN (невалидный уровень через static_cast)
  auto record_unknown = MakeRecord(static_cast<LogLevel>(99), "unknown");
  std::string res_unknown = fmt.Format(record_unknown);
  EXPECT_NE(res_unknown.find("\"level\":\"UNKNOWN\""), std::string::npos);
}
// ============================================================================
// 3. Тесты XmlFormatter
// ============================================================================

TEST(FormattersTest, XmlFormatter_BasicStructure) {
  XmlFormatter fmt;
  auto record = MakeRecord(LogLevel::kWarning, "Warning msg");

  std::string result = fmt.Format(record);

  // Проверка базовой структуры NDXML
  EXPECT_EQ(result.find("<log "), 0);
  EXPECT_NE(result.find("</log>\n"), std::string::npos);
  EXPECT_NE(result.find("level=\"WARNING\""), std::string::npos);
  EXPECT_NE(result.find(">Warning msg</log>"), std::string::npos);
}

TEST(FormattersTest, XmlFormatter_TimeFormat_MillisecondsPadding) {
  XmlFormatter fmt;

  // Создаем время, где миллисекунды равны 15 (т.е. < 100 и >= 10)
  auto tp_15ms =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1015));
  LogRecord record_15{tp_15ms, LogLevel::kInfo, "msg",
                      std::source_location::current()};
  std::string result_15 = fmt.Format(record_15);
  EXPECT_NE(result_15.find(".015"), std::string::npos)
      << "Milliseconds should be padded to 015";

  // Создаем время, где миллисекунды равны 5 (т.е. < 10)
  auto tp_5ms =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1005));
  LogRecord record_5{tp_5ms, LogLevel::kInfo, "msg",
                     std::source_location::current()};
  std::string result_5 = fmt.Format(record_5);
  EXPECT_NE(result_5.find(".005"), std::string::npos)
      << "Milliseconds should be padded to 005";
}

TEST(FormattersTest, XmlFormatter_EscapeSpecialChars) {
  XmlFormatter fmt;
  // Сообщение с символами, имеющими специальное значение в XML
  auto record = MakeRecord(LogLevel::kInfo, "A & B < C > D \"E\" 'F'");

  std::string result = fmt.Format(record);

  EXPECT_NE(result.find("&amp;"), std::string::npos)
      << "Ampersand must be escaped";
  EXPECT_NE(result.find("&lt;"), std::string::npos)
      << "Less-than must be escaped";
  EXPECT_NE(result.find("&gt;"), std::string::npos)
      << "Greater-than must be escaped";
  EXPECT_NE(result.find("&quot;"), std::string::npos)
      << "Double quote must be escaped";
  EXPECT_NE(result.find("&apos;"), std::string::npos)
      << "Single quote must be escaped";
}

TEST(FormattersTest, XmlFormatter_NoEscapingForNormalText) {
  XmlFormatter fmt;
  auto record = MakeRecord(LogLevel::kTrace, "Normal text 123");

  std::string result = fmt.Format(record);

  // Убеждаемся, что обычные символы не были искажены
  EXPECT_NE(result.find(">Normal text 123</log>"), std::string::npos);
}

TEST(FormattersTest, XmlFormatter_LevelStrings_AllLevels) {
  XmlFormatter fmt;

  // Проверка DEBUG
  auto record_debug = MakeRecord(LogLevel::kDebug, "debug");
  std::string res_debug = fmt.Format(record_debug);
  EXPECT_NE(res_debug.find("level=\"DEBUG\""), std::string::npos);

  // Проверка ERROR
  auto record_error = MakeRecord(LogLevel::kError, "error");
  std::string res_error = fmt.Format(record_error);
  EXPECT_NE(res_error.find("level=\"ERROR\""), std::string::npos);

  // Проверка CRITICAL
  auto record_critical = MakeRecord(LogLevel::kCritical, "critical");
  std::string res_critical = fmt.Format(record_critical);
  EXPECT_NE(res_critical.find("level=\"CRITICAL\""), std::string::npos);

  // Проверка UNKNOWN (ветка default)
  auto record_unknown = MakeRecord(static_cast<LogLevel>(99), "unknown");
  std::string res_unknown = fmt.Format(record_unknown);
  EXPECT_NE(res_unknown.find("level=\"UNKNOWN\""), std::string::npos);
}

}  // namespace stc::logger::test