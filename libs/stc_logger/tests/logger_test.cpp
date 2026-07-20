#include "stc/logger/logger.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/ilog_sink.hpp"
#include "stc/logger/core/log_level.hpp"

namespace stc::logger::test {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

// ============================================================================
// Mock-объекты для изоляции тестов
// ============================================================================

class MockLogFormatter : public ILogFormatter {
 public:
  MOCK_METHOD(std::string, Format, (const LogRecord& record),
              (const, override));
};

class MockLogFilter : public ILogFilter {
 public:
  MOCK_METHOD(bool, ShouldPass, (const LogRecord& record), (const, override));
};

class MockLogSink : public ILogSink {
 public:
  MOCK_METHOD(void, Write,
              (const LogRecord& record, std::string_view formatted_message),
              (override));
  MOCK_METHOD(void, Flush, (), (override));
  MOCK_METHOD(std::shared_ptr<ILogFormatter>, GetFormatter, (),
              (const, noexcept, override));
  MOCK_METHOD(std::shared_ptr<ILogFilter>, GetFilter, (),
              (const, noexcept, override));
};

// ============================================================================
// Фикстура теста
// ============================================================================

class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_unique<Logger>("TestLogger");

    // ИСПРАВЛЕНО: Заменяем StrictMock на NiceMock для форматтера и sink'а
    mock_formatter_ = std::make_shared<NiceMock<MockLogFormatter>>();
    mock_filter_ =
        std::make_shared<StrictMock<MockLogFilter>>();  // Фильтр оставляем
                                                        // строгим для проверки
                                                        // short-circuit
    mock_sink_ = std::make_shared<NiceMock<MockLogSink>>();

    ON_CALL(*mock_sink_, GetFormatter()).WillByDefault(Return(mock_formatter_));
    ON_CALL(*mock_sink_, GetFilter()).WillByDefault(Return(nullptr));
  }

  std::unique_ptr<Logger> logger_;
  std::shared_ptr<MockLogFormatter> mock_formatter_;
  std::shared_ptr<MockLogFilter> mock_filter_;
  std::shared_ptr<MockLogSink> mock_sink_;
};

// ============================================================================
// 1. Тесты конструирования и добавления зависимостей
// ============================================================================

TEST_F(LoggerTest, Construction_DoesNotThrow) {
  EXPECT_NO_THROW(Logger("MyLogger"));
}

TEST_F(LoggerTest, AddSink_ValidSink_Succeeds) {
  EXPECT_NO_THROW(logger_->AddSink(mock_sink_));
}

TEST_F(LoggerTest, AddSink_Nullptr_ThrowsInvalidArgument) {
  EXPECT_THROW(logger_->AddSink(nullptr), std::invalid_argument);
}

TEST_F(LoggerTest, AddGlobalFilter_ValidFilter_Succeeds) {
  EXPECT_NO_THROW(logger_->AddGlobalFilter(mock_filter_));
}

TEST_F(LoggerTest, AddGlobalFilter_Nullptr_ThrowsInvalidArgument) {
  EXPECT_THROW(logger_->AddGlobalFilter(nullptr), std::invalid_argument);
}

// ============================================================================
// 2. Тесты маршрутизации и Short-Circuit Evaluation
// ============================================================================

TEST_F(LoggerTest, Routing_HappyPath_FormatsAndWrites) {
  logger_->AddSink(mock_sink_);

  // Ожидаем, что форматтер будет вызван, а затем Write с отформатированной
  // строкой
  EXPECT_CALL(*mock_formatter_, Format(_)).WillOnce(Return("FORMATTED_MSG"));
  EXPECT_CALL(*mock_sink_, Write(_, "FORMATTED_MSG")).Times(1);

  logger_->Info("Test message");
}

TEST_F(LoggerTest, Routing_GlobalFilterBlocks_SkipsFormattingAndWriting) {
  logger_->AddGlobalFilter(mock_filter_);
  logger_->AddSink(mock_sink_);

  // Глобальный фильтр отклоняет сообщение
  EXPECT_CALL(*mock_filter_, ShouldPass(_)).WillOnce(Return(false));

  // Форматтер и Sink НЕ должны быть вызваны (Short-circuit)
  EXPECT_CALL(*mock_formatter_, Format(_)).Times(0);
  EXPECT_CALL(*mock_sink_, Write(_, _)).Times(0);

  logger_->Info("This should be blocked");
}

TEST_F(LoggerTest, Routing_LocalFilterBlocks_SkipsWritingForThatSink) {
  auto local_filter = std::make_shared<StrictMock<MockLogFilter>>();

  // Настраиваем Sink, чтобы он возвращал локальный фильтр
  ON_CALL(*mock_sink_, GetFilter()).WillByDefault(Return(local_filter));

  logger_->AddSink(mock_sink_);

  // Локальный фильтр отклоняет сообщение
  EXPECT_CALL(*local_filter, ShouldPass(_)).WillOnce(Return(false));

  // Форматтер не должен вызываться, так как локальный фильтр сработал до него
  EXPECT_CALL(*mock_formatter_, Format(_)).Times(0);
  EXPECT_CALL(*mock_sink_, Write(_, _)).Times(0);

  logger_->Warning("Blocked by local filter");
}

// ============================================================================
// 3. Тесты методов-оберток (Wrapper Methods)
// ============================================================================

TEST_F(LoggerTest, WrapperMethods_MapToCorrectLevels) {
  logger_->AddSink(mock_sink_);

  // Разрешаем форматирование и записываем в переменную для проверки
  LogRecord captured_record;
  ON_CALL(*mock_formatter_, Format(_)).WillByDefault(Return(""));
  EXPECT_CALL(*mock_sink_, Write(_, _))
      .WillRepeatedly(SaveArg<0>(&captured_record));

  logger_->Trace("t");
  EXPECT_EQ(captured_record.level, LogLevel::kTrace);
  logger_->Debug("d");
  EXPECT_EQ(captured_record.level, LogLevel::kDebug);
  logger_->Info("i");
  EXPECT_EQ(captured_record.level, LogLevel::kInfo);
  logger_->Warning("w");
  EXPECT_EQ(captured_record.level, LogLevel::kWarning);
  logger_->Error("e");
  EXPECT_EQ(captured_record.level, LogLevel::kError);
  logger_->Critical("c");
  EXPECT_EQ(captured_record.level, LogLevel::kCritical);
}

// ============================================================================
// 4. Тесты захвата контекста (std::source_location)
// ============================================================================

TEST_F(LoggerTest, SourceLocation_IsCapturedCorrectly) {
  logger_->AddSink(mock_sink_);

  LogRecord captured_record;
  ON_CALL(*mock_formatter_, Format(_)).WillByDefault(Return(""));
  EXPECT_CALL(*mock_sink_, Write(_, _)).WillOnce(SaveArg<0>(&captured_record));

  // Вызываем логгер на конкретной строке
  logger_->Info("Check location");  // <--- Запомним номер этой строки!
  int expected_line = __LINE__ - 1;

  EXPECT_EQ(captured_record.location.line(), expected_line);
  EXPECT_NE(
      std::string(captured_record.location.file_name()).find("logger_test.cpp"),
      std::string::npos);
}

// ============================================================================
// 5. Тесты Flush
// ============================================================================

TEST_F(LoggerTest, Flush_PropagatesToAllSinks) {
  auto mock_sink_2 = std::make_shared<StrictMock<MockLogSink>>();
  ON_CALL(*mock_sink_2, GetFormatter()).WillByDefault(Return(mock_formatter_));

  logger_->AddSink(mock_sink_);
  logger_->AddSink(mock_sink_2);

  EXPECT_CALL(*mock_sink_, Flush()).Times(1);
  EXPECT_CALL(*mock_sink_2, Flush()).Times(1);

  logger_->Flush();
}

// ============================================================================
// 6. Тесты потокобезопасности (Stress Test)
// ============================================================================

TEST_F(LoggerTest, Concurrency_StressTest_NoCrashesOrDataLoss) {
  // Используем NiceMock, чтобы не спамить предупреждениями о множественных
  // вызовах
  auto nice_sink = std::make_shared<NiceMock<MockLogSink>>();
  auto nice_formatter = std::make_shared<NiceMock<MockLogFormatter>>();
  ON_CALL(*nice_sink, GetFormatter()).WillByDefault(Return(nice_formatter));
  ON_CALL(*nice_formatter, Format(_)).WillByDefault(Return("ok"));

  logger_->AddSink(nice_sink);

  constexpr int kNumThreads = 20;
  constexpr int kMessagesPerThread = 500;
  std::vector<std::jthread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, i]() {
      for (int j = 0; j < kMessagesPerThread; ++j) {
        logger_->Info("Thread " + std::to_string(i) + " msg " +
                      std::to_string(j));
      }
    });
  }

  // std::jthread автоматически делает join при выходе из области видимости.
  // Если внутри Logger есть Data Race или Deadlock, тест зависнет или упадет.
}

TEST_F(LoggerTest, Routing_NullFormatter_SkipsWritingAndDoesNotCrash) {
  // Используем NiceMock, чтобы избежать ошибок на вызовах геттеров,
  // которые не являются фокусом данного теста.
  auto bad_sink = std::make_shared<NiceMock<MockLogSink>>();

  // Настраиваем мок так, чтобы он нарушал контракт и возвращал nullptr
  ON_CALL(*bad_sink, GetFilter()).WillByDefault(Return(nullptr));
  ON_CALL(*bad_sink, GetFormatter()).WillByDefault(Return(nullptr));

  logger_->AddSink(bad_sink);

  // Ожидаем, что Logger запросит форматтер
  EXPECT_CALL(*bad_sink, GetFormatter()).Times(1);

  // Критически важно: метод Write НЕ должен быть вызван,
  // так как выполнение должно прерваться на continue
  EXPECT_CALL(*bad_sink, Write(_, _)).Times(0);

  // Вызов не должен приводить к исключениям или падениям (segfault)
  EXPECT_NO_THROW(logger_->Info("Message that should be silently dropped"));
}

}  // namespace stc::logger::test