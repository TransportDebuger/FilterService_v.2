/**
@file directory_monitor_test.cpp
@brief Тесты фабрики DirectoryMonitor и эвристики statfs.
@version 1.0.1
@date 2026-07-22
*/
#include "stc/fs/directory_monitor.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/statfs.h>

#include <filesystem>
#include <system_error>

#include "mock_system_calls.hpp"

// Подключение внутренних заголовков для проверки типов через dynamic_cast
#include "inotify_monitor.hpp"
#include "polling_monitor.hpp"

namespace stc::fs {

class DirectoryMonitorTest : public ::testing::Test {
 protected:
  std::string temp_dir_;

  void SetUp() override {
    temp_dir_ =
        (std::filesystem::temp_directory_path() / "stc_fs_dir_test").string();
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }
};

TEST_F(DirectoryMonitorTest, CreateInotifyMonitorForExt4) {
  auto mock_sys = std::make_shared<testing::MockFileSystemSystemCalls>();
  struct statfs fake_stat {};
  fake_stat.f_type = 0xEF53;  // EXT4_SUPER_MAGIC

  EXPECT_CALL(*mock_sys, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  auto monitor = DirectoryMonitor::Create(
      temp_dir_, [](IDirectoryMonitor::Event, const std::string&) {},
      std::chrono::seconds(1), mock_sys);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<InotifyMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, CreatePollingMonitorForSmb) {
  auto mock_sys = std::make_shared<testing::MockFileSystemSystemCalls>();
  struct statfs fake_stat {};
  fake_stat.f_type = 0xFF534D42;  // CIFS_MAGIC_NUMBER

  EXPECT_CALL(*mock_sys, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  auto monitor = DirectoryMonitor::Create(
      temp_dir_, [](IDirectoryMonitor::Event, const std::string&) {},
      std::chrono::seconds(1), mock_sys);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<PollingMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, CreatePollingMonitorForFuse) {
  auto mock_sys = std::make_shared<testing::MockFileSystemSystemCalls>();
  struct statfs fake_stat {};
  fake_stat.f_type = 0x65735546;  // FUSE_SUPER_MAGIC

  EXPECT_CALL(*mock_sys, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  auto monitor = DirectoryMonitor::Create(
      temp_dir_, [](IDirectoryMonitor::Event, const std::string&) {},
      std::chrono::seconds(1), mock_sys);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<PollingMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, ThrowSystemErrorOnStatFsFailure) {
  auto mock_sys = std::make_shared<testing::MockFileSystemSystemCalls>();

  EXPECT_CALL(*mock_sys, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::Return(-1));

  EXPECT_THROW(
      DirectoryMonitor::Create(
          temp_dir_, [](auto, auto) {}, std::chrono::seconds(1), mock_sys),
      std::system_error);
}

}  // namespace stc::fs