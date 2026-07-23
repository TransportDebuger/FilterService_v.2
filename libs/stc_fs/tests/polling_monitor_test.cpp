/**
@file polling_monitor_test.cpp
@brief Тесты универсального монитора PollingMonitor.
@version 1.0.3
@date 2026-07-22
*/
#include "polling_monitor.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#include "stc/fs/i_directory_monitor.hpp"

namespace stc::fs {

class PollingMonitorTest : public ::testing::Test {
 protected:
  std::string temp_dir_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<std::pair<IDirectoryMonitor::Event, std::string>> events_;

  void SetUp() override {
    temp_dir_ = (std::filesystem::temp_directory_path() / "stc_fs_polling_test")
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

// --- Базовые тесты обнаружения событий ---

TEST_F(PollingMonitorTest, DetectsNewFile) {
  PollingMonitor monitor(
      temp_dir_, [this](auto e, const auto& p) { OnEvent(e, p); },
      std::chrono::seconds(1));
  monitor.Start();

  std::string new_file =
      (std::filesystem::path(temp_dir_) / "test.txt").string();
  std::ofstream(new_file).close();

  ASSERT_TRUE(WaitForEvents(1, std::chrono::seconds(3)));
  std::lock_guard<std::mutex> lock(mtx_);
  EXPECT_EQ(events_[0].first, IDirectoryMonitor::Event::Created);
  EXPECT_EQ(events_[0].second, new_file);
  monitor.Stop();
}

TEST_F(PollingMonitorTest, DetectsDeletedFile) {
  std::string existing_file =
      (std::filesystem::path(temp_dir_) / "existing.txt").string();
  std::ofstream(existing_file).close();

  PollingMonitor monitor(
      temp_dir_, [this](auto e, const auto& p) { OnEvent(e, p); },
      std::chrono::seconds(1));
  monitor.Start();

  std::filesystem::remove(existing_file);

  ASSERT_TRUE(WaitForEvents(1, std::chrono::seconds(3)));
  std::lock_guard<std::mutex> lock(mtx_);
  EXPECT_EQ(events_[0].first, IDirectoryMonitor::Event::Deleted);
  EXPECT_EQ(events_[0].second, existing_file);
  monitor.Stop();
}

TEST_F(PollingMonitorTest, DetectsModifiedFile) {
  std::string existing_file =
      (std::filesystem::path(temp_dir_) / "modify.txt").string();
  std::ofstream(existing_file).close();

  PollingMonitor monitor(
      temp_dir_, [this](auto e, const auto& p) { OnEvent(e, p); },
      std::chrono::seconds(1));
  monitor.Start();

  // Принудительно изменяем время модификации файла, чтобы обойти проблемы
  // с низким разрешением last_write_time в WSL2/DrvFs и гарантировать триггер
  // события.
  auto future_time =
      std::filesystem::file_time_type::clock::now() + std::chrono::hours(1);
  std::filesystem::last_write_time(existing_file, future_time);

  ASSERT_TRUE(WaitForEvents(1, std::chrono::seconds(3)));

  std::lock_guard<std::mutex> lock(mtx_);
  EXPECT_EQ(events_[0].first, IDirectoryMonitor::Event::Modified);
  EXPECT_EQ(events_[0].second, existing_file);

  monitor.Stop();
}

// --- Тесты для покрытия строк валидации и обработки ошибок (Строка 22 и 64)
// ---

TEST_F(PollingMonitorTest, ThrowsOnNonExistentPath) {
  // Покрытие строки 22: Path does not exist
  EXPECT_THROW(PollingMonitor(
                   "/non/existent/path/12345", [](auto, auto) {},
                   std::chrono::seconds(1)),
               std::runtime_error);
}

TEST_F(PollingMonitorTest, ThrowsOnFilePathInsteadOfDirectory) {
  // Покрытие строки 22: Path is not a directory
  std::string file_path =
      (std::filesystem::path(temp_dir_) / "not_a_dir.txt").string();
  std::ofstream(file_path).close();

  EXPECT_THROW(PollingMonitor(
                   file_path, [](auto, auto) {}, std::chrono::seconds(1)),
               std::runtime_error);
}

TEST_F(PollingMonitorTest, ThrowsWhenPathDisappears) {
  std::string disappearing_dir =
      (std::filesystem::temp_directory_path() / "stc_fs_disappear_test")
          .string();
  std::filesystem::create_directories(disappearing_dir);

  PollingMonitor monitor(
      disappearing_dir, [](auto, auto) {}, std::chrono::seconds(1));
  monitor.Start();

  std::filesystem::remove_all(disappearing_dir);

  // Ждем 3 секунды: 1 сек на интервал опроса + запас на планировщик ОС и I/O
  std::this_thread::sleep_for(std::chrono::seconds(3));

  auto ex = monitor.GetException();
  ASSERT_NE(ex, nullptr)
      << "Expected an exception to be captured from the background thread";

  try {
    std::rethrow_exception(ex);
    FAIL() << "Expected std::runtime_error";
  } catch (const std::runtime_error& e) {
    EXPECT_THAT(e.what(), ::testing::HasSubstr("Path disappeared"));
  }

  monitor.Stop();
}

}  // namespace stc::fs