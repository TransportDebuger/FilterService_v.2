#include "stc/logger/sinks/file/file_sink.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/log_level.hpp"
#include "stc/logger/core/log_record.hpp"
#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace fs = std::filesystem;
namespace stc::logger::test {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

// ============================================================================
// Mock-объекты
// ============================================================================

class MockFormatter : public ILogFormatter {
 public:
  MOCK_METHOD(std::string, Format, (const LogRecord&), (const, override));
};

class MockRotationPolicy : public IRotationPolicy {
 public:
  MOCK_METHOD(bool, ShouldRotate,
              (std::uint64_t, std::chrono::system_clock::time_point),
              (const, override));
  MOCK_METHOD(std::string, GenerateRotatedFileName,
              (const std::string&, std::chrono::system_clock::time_point),
              (const, override));
  MOCK_METHOD(void, OnRotationCompleted,
              (const std::string&, const std::string&), (override));
  MOCK_METHOD(bool, RequiresArchiving, (), (const, noexcept, override));
};

// ============================================================================
// Helper
// ============================================================================

inline LogRecord MakeRecord(LogLevel level, const std::string& msg) {
  return LogRecord{std::chrono::system_clock::now(), level, msg,
                   std::source_location::current()};
}

inline std::string ReadFile(const fs::path& path) {
  std::ifstream ifs(path);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     (std::istreambuf_iterator<char>()));
}

// ============================================================================
// Фикстура теста
// ============================================================================

class FileSinkTest : public ::testing::Test {
 protected:
  fs::path test_dir_;
  fs::path log_file_;

  void SetUp() override {
    auto unique_id =
        std::chrono::steady_clock::now().time_since_epoch().count();
    test_dir_ = fs::temp_directory_path() /
                ("stc_logger_file_test_" + std::to_string(unique_id));
    fs::create_directories(test_dir_);
    log_file_ = test_dir_ / "test.log";
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(test_dir_, ec);
  }
};

// ============================================================================
// 1. Тесты конструирования и валидации
// ============================================================================

TEST_F(FileSinkTest, Construction_CreatesFile) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  {
    FileSink sink(log_file_.string(), formatter);
    EXPECT_TRUE(fs::exists(log_file_))
        << "File should be created on construction";
  }
}

TEST_F(FileSinkTest, Construction_NullFormatter_Throws) {
  EXPECT_THROW(FileSink(log_file_.string(), nullptr), std::invalid_argument);
}

// ============================================================================
// 2. Тесты базовой записи
// ============================================================================

TEST_F(FileSinkTest, Write_AppendsData) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  ON_CALL(*formatter, Format(_)).WillByDefault(Return("FORMATTED_DATA\n"));

  {
    FileSink sink(log_file_.string(), formatter);
    sink.Write(MakeRecord(LogLevel::kInfo, "msg"), "FORMATTED_DATA\n");
    sink.Flush();
  }

  std::string content = ReadFile(log_file_);
  EXPECT_EQ(content, "FORMATTED_DATA\n");
}

TEST_F(FileSinkTest, Write_TracksSize) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  auto policy = std::make_shared<StrictMock<MockRotationPolicy>>();

  // Настраиваем политику: никогда не ротируем, но проверяем, что размер
  // передается
  ON_CALL(*policy, ShouldRotate(_, _)).WillByDefault(Return(false));
  ON_CALL(*policy, RequiresArchiving()).WillByDefault(Return(true));

  // Ожидаем, что ShouldRotate будет вызван с размером, равным длине сообщения
  EXPECT_CALL(*policy, ShouldRotate(0, _))
      .Times(1);  // "FORMATTED_DATA\n" = 15 bytes

  {
    FileSink sink(log_file_.string(), formatter, nullptr, policy);
    sink.Write(MakeRecord(LogLevel::kInfo, "msg"), "FORMATTED_DATA\n");
  }
}

// ============================================================================
// 3. Тесты ротации (с архивацией)
// ============================================================================

TEST_F(FileSinkTest, Rotation_WithArchiving_RenamesAndCreatesNew) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  auto policy = std::make_shared<NiceMock<MockRotationPolicy>>();
  fs::path rotated_file = test_dir_ / "test.log.1";

  // Первый вызов (размер 0) - ротация не нужна.
  EXPECT_CALL(*policy, ShouldRotate(0, _)).WillRepeatedly(Return(false));
  // Второй вызов (размер 6) - ротация нужна.
  EXPECT_CALL(*policy, ShouldRotate(6, _)).WillRepeatedly(Return(true));

  ON_CALL(*policy, RequiresArchiving()).WillByDefault(Return(true));
  ON_CALL(*policy, GenerateRotatedFileName(_, _))
      .WillByDefault(Return(rotated_file.string()));

  {
    FileSink sink(log_file_.string(), formatter, nullptr, policy);

    // 1-я запись: размер 0, ротации нет. "DATA1\n" уходит в исходный файл.
    sink.Write(MakeRecord(LogLevel::kInfo, "msg1"), "DATA1\n");

    // 2-я запись: размер 6, срабатывает ротация.
    // Файл с "DATA1\n" переименовывается в test.log.1. Создается новый
    // test.log.
    sink.Write(MakeRecord(LogLevel::kInfo, "msg2"), "DATA2\n");

    // Проверка состояния файловой системы
    EXPECT_TRUE(fs::exists(rotated_file)) << "Old file should be renamed";
    EXPECT_TRUE(fs::exists(log_file_)) << "New file should be created";

    std::string old_content = ReadFile(rotated_file);
    EXPECT_EQ(old_content, "DATA1\n");
  }

  std::string new_content = ReadFile(log_file_);
  EXPECT_EQ(new_content, "DATA2\n");
}

// ============================================================================
// 4. Тесты ротации (без архивации - Circular logic)
// ============================================================================

TEST_F(FileSinkTest, Rotation_WithoutArchiving_SkipsRename) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  auto policy = std::make_shared<NiceMock<MockRotationPolicy>>();

  EXPECT_CALL(*policy, ShouldRotate(0, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*policy, ShouldRotate(6, _)).WillRepeatedly(Return(true));

  ON_CALL(*policy, RequiresArchiving()).WillByDefault(Return(false));

  {
    FileSink sink(log_file_.string(), formatter, nullptr, policy);

    // 1-я запись: ротации нет.
    sink.Write(MakeRecord(LogLevel::kInfo, "msg1"), "DATA1\n");

    // 2-я запись: срабатывает логика ротации, но RequiresArchiving() == false.
    // Файл не переименовывается, а просто закрывается и открывается заново (или
    // дописывается).
    sink.Write(MakeRecord(LogLevel::kInfo, "msg2"), "DATA2\n");

    EXPECT_TRUE(fs::exists(log_file_));
    EXPECT_FALSE(fs::exists(test_dir_ / "test.log.1"));
  }

  std::string content = ReadFile(log_file_);
  // Так как архивации не было, а поток открыт в режиме app, данные суммируются.
  EXPECT_EQ(content, "DATA1\nDATA2\n");
}

// ============================================================================
// 5. Тесты Flush
// ============================================================================

TEST_F(FileSinkTest, Flush_DoesNotThrow) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  FileSink sink(log_file_.string(), formatter);

  EXPECT_NO_THROW(sink.Flush());
}

TEST_F(FileSinkTest, OpenFile_InvalidPath_HandlesErrorGracefully) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  // Путь в несуществующую директорию заставит open() завершиться с ошибкой
  fs::path invalid_path =
      fs::temp_directory_path() / "non_existent_dir_xyz" / "test.log";

  // Конструктор не должен выбрасывать исключение, а должен вывести ошибку в
  // std::cerr
  EXPECT_NO_THROW({ FileSink sink(invalid_path.string(), formatter); });
}

TEST_F(FileSinkTest, Rotation_RenameFails_HandlesErrorGracefully) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  auto policy = std::make_shared<NiceMock<MockRotationPolicy>>();

  EXPECT_CALL(*policy, ShouldRotate(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, RequiresArchiving()).WillRepeatedly(Return(true));

  // Возвращаем заведомо невалидный путь (несуществующая директория)
  fs::path invalid_rotated_path =
      fs::temp_directory_path() / "non_existent_dir_abc" / "archive.log";
  EXPECT_CALL(*policy, GenerateRotatedFileName(_, _))
      .WillRepeatedly(Return(invalid_rotated_path.string()));

  FileSink sink(log_file_.string(), formatter, nullptr, policy);

  // Записываем данные, чтобы вызвать срабатывание логики ротации
  sink.Write(MakeRecord(LogLevel::kInfo, "msg"), "DATA\n");
}

TEST_F(FileSinkTest, PolymorphicDeletion_CoversDeletingDestructor) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  // Явное создание через new и удаление через базовый класс
  ILogSink* base_ptr = new FileSink(log_file_.string(), formatter);
  delete base_ptr;  // Вызовет Deleting Destructor (D0)
}

}  // namespace stc::logger::test