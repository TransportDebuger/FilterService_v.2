#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <source_location>
#include <vector>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/log_level.hpp"
#include "stc/logger/core/log_record.hpp"
#include "stc/logger/filters/composite_filter.hpp"
#include "stc/logger/filters/level_filter.hpp"
#include "stc/logger/filters/source_filter.hpp"

namespace stc::logger::test {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

// ============================================================================
// Mock-объект для тестирования CompositeFilter
// ============================================================================
class MockLogFilter : public ILogFilter {
 public:
  MOCK_METHOD(bool, ShouldPass, (const LogRecord& record), (const, override));
};

// ============================================================================
// Фикстура теста
// ============================================================================
class FiltersTest : public ::testing::Test {
 protected:
  // Вспомогательный метод для создания записи лога с заданным уровнем
  // и автоматическим захватом source_location из места вызова.
  LogRecord CreateRecord(LogLevel level, std::source_location loc =
                                             std::source_location::current()) {
    return LogRecord{std::chrono::system_clock::now(), level, "Test message",
                     loc};
  }
};

// ============================================================================
// 1. Тесты LevelFilter
// ============================================================================

TEST_F(FiltersTest, LevelFilter_Pass_WhenLevelIsHigher) {
  LevelFilter filter(LogLevel::kWarning);
  auto record = CreateRecord(LogLevel::kError);
  EXPECT_TRUE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, LevelFilter_Pass_WhenLevelIsEqual) {
  LevelFilter filter(LogLevel::kInfo);
  auto record = CreateRecord(LogLevel::kInfo);
  EXPECT_TRUE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, LevelFilter_Block_WhenLevelIsLower) {
  LevelFilter filter(LogLevel::kWarning);
  auto record = CreateRecord(LogLevel::kDebug);
  EXPECT_FALSE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, LevelFilter_Boundary_Trace) {
  LevelFilter filter(LogLevel::kTrace);
  auto record = CreateRecord(LogLevel::kTrace);
  EXPECT_TRUE(filter.ShouldPass(record));
}

// ============================================================================
// 2. Тесты SourceFilter
// ============================================================================
// Примечание: Так как std::source_location захватывает реальные данные,
// мы используем имя этого файла ("filters_test.cpp") и имена тестов
// (которые содержат "SourceFilter") для проверки.

TEST_F(FiltersTest, SourceFilter_FileName_ExactMatch_Success) {
  auto record = CreateRecord(LogLevel::kInfo);

  // std::source_location::file_name() возвращает полный путь (абсолютный или
  // относительный). Для точного совпадения (kExact) паттерн должен в точности
  // повторять этот путь.
  std::string full_path = record.location.file_name();

  SourceFilter filter(full_path, SourceMatchTarget::kFileName,
                      SourceMatchMode::kExact);
  EXPECT_TRUE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_FileName_ExactMatch_Failure) {
  auto record = CreateRecord(LogLevel::kInfo);
  // Заведомо неверное точное имя
  SourceFilter filter("completely_different_file.cpp",
                      SourceMatchTarget::kFileName, SourceMatchMode::kExact);
  EXPECT_FALSE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_StartsWith_PatternLongerThanSource_Fails) {
  auto record = CreateRecord(LogLevel::kInfo);
  // Шаблон заведомо длиннее любого возможного имени файла или функции в этом
  // тесте
  std::string long_pattern =
      "ThisIsAVeryLongPatternThatWillNeverMatchAnySourceLocation";

  SourceFilter filter(long_pattern, SourceMatchTarget::kFileName,
                      SourceMatchMode::kStartsWith);
  EXPECT_FALSE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_FileName_Contains) {
  SourceFilter filter("filters_test", SourceMatchTarget::kFileName,
                      SourceMatchMode::kContains);
  auto record = CreateRecord(LogLevel::kInfo);
  EXPECT_TRUE(filter.ShouldPass(record));  // Файл называется filters_test.cpp
}

TEST_F(FiltersTest, SourceFilter_FunctionName_StartsWith) {
  auto record = CreateRecord(LogLevel::kInfo);
  // В GCC/Clang имя тестовой функции начинается с "virtual void"
  SourceFilter filter("virtual void", SourceMatchTarget::kFunctionName,
                      SourceMatchMode::kStartsWith);
  EXPECT_TRUE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_FileName_ContainsExactName) {
  auto record = CreateRecord(LogLevel::kInfo);
  // Используем kContains с точным именем файла, чтобы избежать проблем с
  // абсолютными путями
  SourceFilter filter("filters_test.cpp", SourceMatchTarget::kFileName,
                      SourceMatchMode::kContains);
  EXPECT_TRUE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_FunctionName_Contains) {
  // Имя тестовой функции содержит подстроку "FunctionName_Contains"
  SourceFilter filter("FunctionName_Contains", SourceMatchTarget::kFunctionName,
                      SourceMatchMode::kContains);
  auto record = CreateRecord(LogLevel::kInfo);
  EXPECT_TRUE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_Inversion_Blacklist) {
  // invert = true означает "пропускать всё, КРОМЕ совпадений" (Blacklist)
  SourceFilter filter("filters_test", SourceMatchTarget::kFileName,
                      SourceMatchMode::kContains, true);
  auto record = CreateRecord(LogLevel::kInfo);

  // Так как имя файла совпадает с "filters_test", а мы в режиме Blacklist,
  // фильтр должен отклонить (вернуть false) эту запись.
  EXPECT_FALSE(filter.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_NoMatch_WithInversion) {
  // Ищем несуществующую строку в режиме Blacklist (invert = true)
  SourceFilter filter("non_existent_string", SourceMatchTarget::kFileName,
                      SourceMatchMode::kContains, true);
  auto record = CreateRecord(LogLevel::kInfo);

  // Совпадения нет -> в режиме Blacklist это означает "пропустить"
  EXPECT_TRUE(filter.ShouldPass(record));
}

// ============================================================================
// 3. Тесты CompositeFilter
// ============================================================================

TEST_F(FiltersTest, CompositeFilter_EmptyVector_Throws) {
  std::vector<std::shared_ptr<ILogFilter>> empty_filters;
  EXPECT_THROW(CompositeFilter(empty_filters, LogicOperator::kAnd),
               std::invalid_argument);
}

TEST_F(FiltersTest, CompositeFilter_And_ShortCircuit) {
  auto mock1 = std::make_shared<StrictMock<MockLogFilter>>();
  auto mock2 = std::make_shared<StrictMock<MockLogFilter>>();

  std::vector<std::shared_ptr<ILogFilter>> filters = {mock1, mock2};
  CompositeFilter composite(filters, LogicOperator::kAnd);

  auto record = CreateRecord(LogLevel::kInfo);

  // ИСПРАВЛЕНО: Используем _ вместо record
  EXPECT_CALL(*mock1, ShouldPass(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock2, ShouldPass(_)).Times(0);

  EXPECT_FALSE(composite.ShouldPass(record));
}

TEST_F(FiltersTest, CompositeFilter_Or_ShortCircuit) {
  auto mock1 = std::make_shared<StrictMock<MockLogFilter>>();
  auto mock2 = std::make_shared<StrictMock<MockLogFilter>>();

  std::vector<std::shared_ptr<ILogFilter>> filters = {mock1, mock2};
  CompositeFilter composite(filters, LogicOperator::kOr);

  auto record = CreateRecord(LogLevel::kInfo);

  EXPECT_CALL(*mock1, ShouldPass(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock2, ShouldPass(_)).Times(0);

  EXPECT_TRUE(composite.ShouldPass(record));
}

TEST_F(FiltersTest, CompositeFilter_And_AllPass) {
  auto mock1 = std::make_shared<StrictMock<MockLogFilter>>();
  auto mock2 = std::make_shared<StrictMock<MockLogFilter>>();

  std::vector<std::shared_ptr<ILogFilter>> filters = {mock1, mock2};
  CompositeFilter composite(filters, LogicOperator::kAnd);

  auto record = CreateRecord(LogLevel::kInfo);

  EXPECT_CALL(*mock1, ShouldPass(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock2, ShouldPass(_)).WillOnce(Return(true));

  EXPECT_TRUE(composite.ShouldPass(record));
}

TEST_F(FiltersTest, CompositeFilter_Or_AllFail) {
  auto mock1 = std::make_shared<StrictMock<MockLogFilter>>();
  auto mock2 = std::make_shared<StrictMock<MockLogFilter>>();

  std::vector<std::shared_ptr<ILogFilter>> filters = {mock1, mock2};
  CompositeFilter composite(filters, LogicOperator::kOr);

  auto record = CreateRecord(LogLevel::kInfo);

  EXPECT_CALL(*mock1, ShouldPass(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock2, ShouldPass(_)).WillOnce(Return(false));

  EXPECT_FALSE(composite.ShouldPass(record));
}

TEST_F(FiltersTest, SourceFilter_InvalidMatchMode_ReturnsFalse) {
  auto record = CreateRecord(LogLevel::kInfo);

  // Форсируем невалидное значение enum для проверки ветки 'default' в switch
  SourceMatchMode invalid_mode = static_cast<SourceMatchMode>(99);

  SourceFilter filter("dummy_pattern", SourceMatchTarget::kFileName,
                      invalid_mode);

  // Ожидаем, что защитный механизм вернет false, а не упадет или вернет true
  EXPECT_FALSE(filter.ShouldPass(record));
}

}  // namespace stc::logger::test