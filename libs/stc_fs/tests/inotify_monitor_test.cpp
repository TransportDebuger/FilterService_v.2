/**
@file inotify_monitor_test.cpp
@brief Тесты событийного монитора InotifyMonitor.
@version 1.0.2
@date 2026-07-22
*/
#include "inotify_monitor.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/statfs.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#include "mock_system_calls.hpp"
#include "native_inotify_system_calls.hpp"
#include "stc/fs/i_directory_monitor.hpp"

namespace stc::fs {

class InotifyMonitorTest : public ::testing::Test {
 protected:
  std::string temp_dir_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<std::pair<IDirectoryMonitor::Event, std::string>> events_;

  void SetUp() override {
    temp_dir_ = (std::filesystem::temp_directory_path() / "stc_fs_inotify_test")
                    .string();
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  void OnEvent(IDirectoryMonitor::Event event, const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    events_.emplace_back(event, path);
    cv_.notify_all();
  }

  bool WaitForEvents(size_t count, std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, timeout,
                        [this, count] { return events_.size() >= count; });
  }
};

TEST_F(InotifyMonitorTest, DetectsNewFile) {
  auto sys_calls = std::make_shared<NativeInotifySystemCalls>();
  InotifyMonitor monitor(
      temp_dir_, [this](auto e, const auto& p) { OnEvent(e, p); }, sys_calls);
  monitor.Start();

  std::string new_file =
      (std::filesystem::path(temp_dir_) / "test.txt").string();
  std::ofstream(new_file).close();

  ASSERT_TRUE(WaitForEvents(1, std::chrono::seconds(2)));
  std::lock_guard<std::mutex> lock(mtx_);
  EXPECT_EQ(events_[0].first, IDirectoryMonitor::Event::Created);
  EXPECT_EQ(events_[0].second, new_file);
  monitor.Stop();
}

TEST_F(InotifyMonitorTest, DetectsDeletedFile) {
  std::string existing_file =
      (std::filesystem::path(temp_dir_) / "existing.txt").string();
  std::ofstream(existing_file).close();

  auto sys_calls = std::make_shared<NativeInotifySystemCalls>();
  InotifyMonitor monitor(
      temp_dir_, [this](auto e, const auto& p) { OnEvent(e, p); }, sys_calls);
  monitor.Start();

  std::filesystem::remove(existing_file);

  ASSERT_TRUE(WaitForEvents(1, std::chrono::seconds(2)));
  std::lock_guard<std::mutex> lock(mtx_);
  EXPECT_EQ(events_[0].first, IDirectoryMonitor::Event::Deleted);
  EXPECT_EQ(events_[0].second, existing_file);
  monitor.Stop();
}

TEST_F(InotifyMonitorTest, DetectsModifiedFile) {
  std::string existing_file =
      (std::filesystem::path(temp_dir_) / "modify.txt").string();
  std::ofstream(existing_file).close();

  auto sys_calls = std::make_shared<NativeInotifySystemCalls>();
  InotifyMonitor monitor(
      temp_dir_, [this](auto e, const auto& p) { OnEvent(e, p); }, sys_calls);
  monitor.Start();

  std::ofstream(existing_file, std::ios::app) << "data";

  ASSERT_TRUE(WaitForEvents(1, std::chrono::seconds(2)));
  std::lock_guard<std::mutex> lock(mtx_);
  EXPECT_EQ(events_[0].first, IDirectoryMonitor::Event::Modified);
  EXPECT_EQ(events_[0].second, existing_file);
  monitor.Stop();
}

// --- Тесты для покрытия строк обработки ошибок и валидации ---

TEST_F(InotifyMonitorTest, ThrowsOnNullSystemCalls) {
  // Покрытие строки: System calls interface cannot be null
  EXPECT_THROW(InotifyMonitor(
                   temp_dir_, [](auto, auto) {}, nullptr),
               std::invalid_argument);
}

TEST_F(InotifyMonitorTest, ThrowsOnInitFailure) {
  // Покрытие строк: Failed to initialize inotify
  auto mock_sys =
      std::make_shared<stc::fs::testing::MockFileSystemSystemCalls>();

  EXPECT_CALL(*mock_sys, Init()).WillOnce(::testing::Return(-1));

  errno = EMFILE;

  EXPECT_THROW(InotifyMonitor(
                   temp_dir_, [](auto, auto) {}, mock_sys)
                   .Start(),
               std::system_error);
}

TEST_F(InotifyMonitorTest, ThrowsOnAddWatchFailure) {
  // Покрытие строк: Failed to add watch (и корректный Close inotify_fd)
  auto mock_sys =
      std::make_shared<stc::fs::testing::MockFileSystemSystemCalls>();

  EXPECT_CALL(*mock_sys, Init())
      .WillOnce(::testing::Return(5));  // Успешный init, вернули fd = 5

  EXPECT_CALL(*mock_sys, AddWatch(5, temp_dir_, ::testing::_))
      .WillOnce(::testing::Return(-1));  // Ошибка add_watch

  // Ожидаем, что при ошибке add_watch будет вызван Close для освобождения
  // ресурса
  EXPECT_CALL(*mock_sys, Close(5)).Times(1);

  errno = ENOSPC;

  InotifyMonitor monitor(
      temp_dir_, [](auto, auto) {}, mock_sys);

  EXPECT_THROW(monitor.Start(), std::system_error);
}

TEST_F(InotifyMonitorTest, ThrowsOnReadFailure) {
  // Покрытие строки: read failed
  auto mock_sys =
      std::make_shared<stc::fs::testing::MockFileSystemSystemCalls>();

  EXPECT_CALL(*mock_sys, Init()).WillOnce(::testing::Return(5));

  EXPECT_CALL(*mock_sys, AddWatch(5, temp_dir_, ::testing::_))
      .WillOnce(::testing::Return(10));  // Успешный add_watch

  // Симулируем критическую ошибку чтения (не EAGAIN, а например EFAULT)
  EXPECT_CALL(*mock_sys, Read(5, ::testing::_, ::testing::_))
      .WillRepeatedly(::testing::DoAll(
          ::testing::Invoke([](int, void*, size_t) { errno = EFAULT; }),
          ::testing::Return(-1)));

  // Ожидаем, что поток завершится с исключением, которое будет перехвачено в
  // GetException
  InotifyMonitor monitor(
      temp_dir_, [](auto, auto) {}, mock_sys);
  monitor.Start();

  // Даем фоновому потоку время столкнуться с ошибкой чтения
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto ex = monitor.GetException();
  ASSERT_NE(ex, nullptr)
      << "Expected an exception to be captured from the background thread";

  try {
    std::rethrow_exception(ex);
    FAIL() << "Expected std::system_error";
  } catch (const std::system_error& e) {
    EXPECT_THAT(e.what(), ::testing::HasSubstr("read failed"));
  }

  monitor.Stop();
}

TEST_F(InotifyMonitorTest, NativeSystemCallsDestructorsCoverage) {
  // 1. Покрытие базового деструктора (D2Ev)
  // Создание объекта на стеке. Деструктор вызывается при выходе из блока.
  {
    NativeInotifySystemCalls sys_stack;
    (void)sys_stack;  // Подавляем предупреждение об неиспользуемой переменной
  }

  // 2. Покрытие deleting деструктора (D0Ev)
  // Явное создание в куче и удаление через указатель на базовый класс.
  // Это единственный способ заставить компилятор сгенерировать и gcov
  // зафиксировать D0Ev.
  IFileSystemSystemCalls* base_ptr = new NativeInotifySystemCalls();
  delete base_ptr;
}

TEST_F(InotifyMonitorTest, NativeSystemCallsStatFsAndDestruction) {
  struct statfs buf {};
  {
    // Инстанцируем нативную реализацию на стеке
    NativeInotifySystemCalls sys;

    // Вызываем StatFs для реальной временной директории
    int result = sys.StatFs(temp_dir_, &buf);

    // Проверяем успешность выполнения и корректность заполнения буфера
    EXPECT_EQ(result, 0);
    EXPECT_NE(buf.f_type, 0);  // f_type не должен быть нулевым для валидной ФС
  }  // <-- При выходе из блока объект `sys` уничтожается,
     //     что гарантирует вызов деструктора ~NativeInotifySystemCalls()
}

}  // namespace stc::fs