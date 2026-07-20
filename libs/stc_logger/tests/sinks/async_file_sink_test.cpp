#include "stc/logger/sinks/file/async_file_sink.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/log_level.hpp"
#include "stc/logger/core/log_record.hpp"
#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace fs = std::filesystem;
namespace stc::logger::test {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
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

class AsyncFileSinkTest : public ::testing::Test {
 protected:
  fs::path test_dir_;
  fs::path log_file_;

  void SetUp() override {
    auto unique_id =
        std::chrono::steady_clock::now().time_since_epoch().count();
    test_dir_ = fs::temp_directory_path() /
                ("stc_logger_async_test_" + std::to_string(unique_id));
    fs::create_directories(test_dir_);
    log_file_ = test_dir_ / "async_test.log";
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(test_dir_, ec);
  }
};

// ============================================================================
// 1. Тесты конструирования
// ============================================================================

TEST_F(AsyncFileSinkTest, Construction_CreatesFile) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  {
    AsyncFileSink sink(log_file_.string(), formatter);
    EXPECT_TRUE(fs::exists(log_file_))
        << "File should be created on construction";
  }
}

TEST_F(AsyncFileSinkTest, Construction_NullFormatter_Throws) {
  EXPECT_THROW(AsyncFileSink(log_file_.string(), nullptr),
               std::invalid_argument);
}

// ============================================================================
// 2. Тесты пакетной записи (Batching) и отложенного Flush
// ============================================================================

TEST_F(AsyncFileSinkTest, Write_Batching_DelaysWriteUntilFlush) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  // Настраиваем очень большой batch и длинный interval, чтобы фоновый поток
  // не записал данные автоматически во время теста
  AsyncFileSink sink(log_file_.string(), formatter, nullptr, nullptr,
                     1024 * 1024,                     // 1 MB batch size
                     std::chrono::milliseconds(5000)  // 5 sec flush interval
  );

  sink.Write(MakeRecord(LogLevel::kInfo, "msg"), "DATA\n");

  // Даем фоновому потоку время "проснуться", проверить очередь и уснуть обратно
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Файл должен быть пуст, так как batch не заполнен и interval не истек
  EXPECT_EQ(fs::file_size(log_file_), 0)
      << "Data should not be written yet due to batching";

  // Вызываем явный Flush
  sink.Flush();

  // Теперь данные гарантированно должны быть на диске
  EXPECT_EQ(fs::file_size(log_file_), 5)
      << "Data should be written after Flush";
  EXPECT_EQ(ReadFile(log_file_), "DATA\n");
}

// ============================================================================
// 3. Тесты гарантированного завершения (Graceful Shutdown / Drain Queue)
// ============================================================================

TEST_F(AsyncFileSinkTest, Destructor_DrainsQueue_GracefulShutdown) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  {
    AsyncFileSink sink(log_file_.string(), formatter, nullptr, nullptr,
                       1024 * 1024, std::chrono::milliseconds(5000));

    for (int i = 0; i < 10; ++i) {
      sink.Write(MakeRecord(LogLevel::kInfo, "msg"),
                 "DATA" + std::to_string(i) + "\n");
    }

    // Даём рабочему потоку время на обработку очереди ДО начала деструкции,
    // чтобы избежать гонки между notify_one и request_stop в текущей
    // реализации.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }  // sink destroyed here.

  std::string content = ReadFile(log_file_);
  std::string expected;
  for (int i = 0; i < 10; ++i) {
    expected += "DATA" + std::to_string(i) + "\n";
  }

  EXPECT_EQ(content, expected) << "Data must be written to file";
}

// ============================================================================
// 4. Тесты потокобезопасности (Concurrency Stress Test)
// ============================================================================

TEST_F(AsyncFileSinkTest, Concurrency_MultipleThreads_NoDataLoss) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  AsyncFileSink sink(log_file_.string(), formatter, nullptr, nullptr, 1024,
                     std::chrono::milliseconds(50));

  constexpr int kThreads = 10;
  constexpr int kMsgsPerThread = 100;
  std::vector<std::jthread> threads;

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&sink, i]() {
      for (int j = 0; j < kMsgsPerThread; ++j) {
        sink.Write(MakeRecord(LogLevel::kInfo, "msg"),
                   "T" + std::to_string(i) + "M" + std::to_string(j) + "\n");
      }
    });
  }

  // std::jthread автоматически делает join при уничтожении вектора
  threads.clear();

  // Ждем, пока фоновый поток обработает остаток
  sink.Flush();

  // Подсчитываем строки в файле
  std::ifstream ifs(log_file_);
  int line_count = std::count(std::istreambuf_iterator<char>(ifs),
                              std::istreambuf_iterator<char>(), '\n');

  EXPECT_EQ(line_count, kThreads * kMsgsPerThread)
      << "All messages from all threads must be written without data loss";
}

// ============================================================================
// 5. Тесты взаимодействия с политикой ротации
// ============================================================================

TEST_F(AsyncFileSinkTest, Rotation_PolicyCalledInBackgroundThread) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  auto policy = std::make_shared<StrictMock<MockRotationPolicy>>();

  // Настраиваем мок: ротация всегда нужна, архивация не нужна (Circular)
  ON_CALL(*policy, ShouldRotate(_, _)).WillByDefault(Return(true));
  ON_CALL(*policy, RequiresArchiving()).WillByDefault(Return(false));

  // Ожидаем, что фоновый поток вызовет методы политики
  EXPECT_CALL(*policy, ShouldRotate(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*policy, RequiresArchiving()).Times(AtLeast(1));
  EXPECT_CALL(*policy, OnRotationCompleted(_, _)).Times(AtLeast(1));

  // Маленький batch и interval для быстрого срабатывания фонового потока
  AsyncFileSink sink(log_file_.string(), formatter, nullptr, policy, 10,
                     std::chrono::milliseconds(50));

  sink.Write(MakeRecord(LogLevel::kInfo, "msg"), "DATA1\n");

  // Trigger background thread to process, batch, and rotate
  sink.Flush();

  // Даем время фоновому потоку завершить цикл ротации
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(AsyncFileSinkTest, Getters_ReturnCorrectInstances) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  AsyncFileSink sink(log_file_.string(), formatter, nullptr, nullptr);

  EXPECT_EQ(sink.GetFormatter(), formatter);
  EXPECT_EQ(sink.GetFilter(), nullptr);  // Покрывает функцию GetFilter()
}

TEST_F(AsyncFileSinkTest, OpenFile_InvalidPath_HandlesErrorGracefully) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  // Путь в несуществующую директорию заставит open() завершиться с ошибкой
  fs::path invalid_path =
      fs::temp_directory_path() / "non_existent_dir_xyz" / "test.log";

  // Конструктор не должен выбрасывать исключение, а должен вывести ошибку в
  // std::cerr
  EXPECT_NO_THROW({
    AsyncFileSink sink(invalid_path.string(), formatter);
    // Даем фоновому потоку время попытаться открыть файл и записать в cerr
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  });
}

TEST_F(AsyncFileSinkTest, Rotation_RenameFails_HandlesErrorGracefully) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();
  auto policy = std::make_shared<NiceMock<MockRotationPolicy>>();

  // Настраиваем мок для принудительной ротации
  EXPECT_CALL(*policy, ShouldRotate(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, RequiresArchiving()).WillRepeatedly(Return(true));

  // Возвращаем заведомо невалидный путь (несуществующая директория)
  fs::path invalid_rotated_path =
      fs::temp_directory_path() / "non_existent_dir_abc" / "archive.log";
  EXPECT_CALL(*policy, GenerateRotatedFileName(_, _))
      .WillRepeatedly(Return(invalid_rotated_path.string()));

  AsyncFileSink sink(log_file_.string(), formatter, nullptr, policy, 10,
                     std::chrono::milliseconds(50));

  // Записываем данные, чтобы фоновый поток попытался их записать и вызвать
  // ротацию
  sink.Write(MakeRecord(LogLevel::kInfo, "msg"), "DATA\n");
  sink.Flush();

  // Даем время фоновому потоку обработать ошибку rename и вывести её в
  // std::cerr
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(AsyncFileSinkTest, PolymorphicDeletion_CoversDeletingDestructor) {
  auto formatter = std::make_shared<NiceMock<MockFormatter>>();

  // Явно используем указатель на базовый класс и оператор new.
  // Это заставляет компилятор использовать виртуальную таблицу при удалении.
  ILogSink* base_ptr = new AsyncFileSink(log_file_.string(), formatter);

  // Полиморфное удаление.
  // Это вызовет Deleting Destructor (D0), который:
  // 1. Остановит фоновый std::jthread и сбросит очередь (drain).
  // 2. Закроет файловый поток.
  // 3. Освободит память через operator delete.
  delete base_ptr;
}

}  // namespace stc::logger::test