/**
@file directory_monitor_test.cpp
@brief Тесты фабрики DirectoryMonitor и эвристики statfs.
@version 1.0.2
@date 2026-07-24
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
  std::shared_ptr<testing::MockFileSystemSystemCalls> mock_sys_calls_;
  IDirectoryMonitor::Callback dummy_callback_;

  void SetUp() override {
    temp_dir_ =
        (std::filesystem::temp_directory_path() / "stc_fs_dir_test").string();
    std::filesystem::create_directories(temp_dir_);

    mock_sys_calls_ = std::make_shared<testing::MockFileSystemSystemCalls>();
    dummy_callback_ = [](IDirectoryMonitor::Event, const std::string&) {};
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }
};

// ============================================================================
// Тесты автоматического выбора стратегии (Create)
// ============================================================================

TEST_F(DirectoryMonitorTest, CreateInotifyMonitorForExt4) {
  struct statfs fake_stat {};
  fake_stat.f_type = 0xEF53;  // EXT4_SUPER_MAGIC

  EXPECT_CALL(*mock_sys_calls_, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  auto monitor = DirectoryMonitor::Create(
      temp_dir_, dummy_callback_, std::chrono::seconds(1), mock_sys_calls_);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<InotifyMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, CreatePollingMonitorForSmb) {
  struct statfs fake_stat {};
  fake_stat.f_type = 0xFF534D42;  // CIFS_MAGIC_NUMBER

  EXPECT_CALL(*mock_sys_calls_, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  auto monitor = DirectoryMonitor::Create(
      temp_dir_, dummy_callback_, std::chrono::seconds(1), mock_sys_calls_);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<PollingMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, CreatePollingMonitorForFuse) {
  struct statfs fake_stat {};
  fake_stat.f_type = 0x65735546;  // FUSE_SUPER_MAGIC

  EXPECT_CALL(*mock_sys_calls_, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  auto monitor = DirectoryMonitor::Create(
      temp_dir_, dummy_callback_, std::chrono::seconds(1), mock_sys_calls_);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<PollingMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, ThrowSystemErrorOnStatFsFailure) {
  EXPECT_CALL(*mock_sys_calls_, StatFs(temp_dir_, ::testing::_))
      .WillOnce(::testing::Return(-1));

  EXPECT_THROW(
      DirectoryMonitor::Create(temp_dir_, dummy_callback_,
                               std::chrono::seconds(1), mock_sys_calls_),
      std::system_error);
}

// ============================================================================
// Тесты явного выбора стратегии (CreateWithStrategy)
// ============================================================================

TEST_F(DirectoryMonitorTest, CreateWithStrategy_Inotify_ReturnsInotifyMonitor) {
  auto monitor = DirectoryMonitor::CreateWithStrategy(
      DirectoryMonitor::MonitoringStrategy::Inotify, temp_dir_, dummy_callback_,
      std::chrono::seconds(5), mock_sys_calls_);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<InotifyMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest, CreateWithStrategy_Polling_ReturnsPollingMonitor) {
  auto monitor = DirectoryMonitor::CreateWithStrategy(
      DirectoryMonitor::MonitoringStrategy::Polling, temp_dir_, dummy_callback_,
      std::chrono::seconds(5), mock_sys_calls_);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<PollingMonitor*>(monitor.get()), nullptr);
}

TEST_F(DirectoryMonitorTest,
       CreateWithStrategy_Auto_DelegatesToCreateAndCallsStatFs) {
  // Настраиваем Mock для возврата локальной ФС (EXT4)
  struct statfs fake_stat {};
  fake_stat.f_type = 0xEF53;  // EXT4_SUPER_MAGIC

  EXPECT_CALL(*mock_sys_calls_, StatFs(::testing::_, ::testing::_))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(fake_stat),
                                 ::testing::Return(0)));

  // Вызов с Auto должен успешно делегировать вызов в Create() и вернуть
  // InotifyMonitor
  auto monitor = DirectoryMonitor::CreateWithStrategy(
      DirectoryMonitor::MonitoringStrategy::Auto, temp_dir_, dummy_callback_,
      std::chrono::seconds(5), mock_sys_calls_);

  ASSERT_NE(monitor, nullptr);
  EXPECT_NE(dynamic_cast<InotifyMonitor*>(monitor.get()), nullptr);
}

}  // namespace stc::fs