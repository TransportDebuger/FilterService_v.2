#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "stc/DaemonManager.hpp"

namespace fs = std::filesystem;

// Мок-класс для системных вызовов
class SysCallWrapper {
public:
    virtual ~SysCallWrapper() = default;
    virtual pid_t fork() = 0;
    virtual int setsid() = 0;
    virtual int chdir(const char* path) = 0;
    virtual int chmod(const char* path, mode_t mode) = 0;
    virtual int close(int fd) = 0;
    virtual int kill(pid_t pid, int sig) = 0;
};

class MockSysCallWrapper : public SysCallWrapper {
public:
    MOCK_METHOD(pid_t, fork, (), (override));
    MOCK_METHOD(int, setsid, (), (override));
    MOCK_METHOD(int, chdir, (const char*), (override));
    MOCK_METHOD(int, chmod, (const char*, mode_t), (override));
    MOCK_METHOD(int, close, (int), (override));
    MOCK_METHOD(int, kill, (pid_t, int), (override));
};

class DaemonManagerTest : public ::testing::Test {
protected:
    const std::string pidFilePath = "/tmp/test_daemon.pid";
    MockSysCallWrapper* mock_sys_;

    void SetUp() override {
        mock_sys_ = new MockSysCallWrapper();
        stc::DaemonManager::setSysCallWrapper(mock_sys_);
        
        // Очистка PID-файла перед каждым тестом
        if (fs::exists(pidFilePath)) {
            fs::remove(pidFilePath);
        }
    }

    void TearDown() override {
        stc::DaemonManager::resetSysCallWrapper();
        delete mock_sys_;
    }
};

TEST_F(DaemonManagerTest, ConstructorCreatesAndRemovesPidFile) {
    {
        stc::DaemonManager dm(pidFilePath);
        dm.daemonize();
        dm.writePid();
        ASSERT_TRUE(fs::exists(pidFilePath));
        
        std::ifstream file(pidFilePath);
        pid_t pid;
        file >> pid;
        ASSERT_EQ(pid, getpid());
    }
    ASSERT_FALSE(fs::exists(pidFilePath));
}

TEST_F(DaemonManagerTest, WritePidThrowsOnInvalidPath) {
    stc::DaemonManager dm("/invalid_directory/test.pid");
    EXPECT_CALL(*mock_sys_, fork()).WillOnce(Return(0));
    dm.daemonize();
    EXPECT_THROW(dm.writePid(), std::system_error);
}

TEST_F(DaemonManagerTest, DetectRunningProcess) {
    std::ofstream(pidFilePath) << getpid();
    EXPECT_CALL(*mock_sys_, kill(getpid(), 0)).WillOnce(Return(0));
    EXPECT_THROW(stc::DaemonManager dm(pidFilePath), std::runtime_error);
}

TEST_F(DaemonManagerTest, HandleStalePidFile) {
    const pid_t fakePid = 99999;
    std::ofstream(pidFilePath) << fakePid;
    
    EXPECT_CALL(*mock_sys_, kill(fakePid, 0))
        .WillOnce(Return(-1));
        
    EXPECT_NO_THROW(stc::DaemonManager dm(pidFilePath));
    EXPECT_FALSE(fs::exists(pidFilePath));
}

TEST_F(DaemonManagerTest, DaemonizeSuccessFlow) {
    EXPECT_CALL(*mock_sys_, fork())
        .WillOnce(Return(123))  // Первый fork
        .WillOnce(Return(0));   // Второй fork
    
    EXPECT_CALL(*mock_sys_, setsid()).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys_, chdir("/")).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys_, close(_)).Times(3).WillRepeatedly(Return(0));

    stc::DaemonManager dm(pidFilePath);
    EXPECT_NO_THROW(dm.daemonize());
}

TEST_F(DaemonManagerTest, FirstForkFailure) {
    EXPECT_CALL(*mock_sys_, fork())
        .WillOnce(SetErrnoAndReturn(EAGAIN, -1));
    
    stc::DaemonManager dm(pidFilePath);
    EXPECT_THROW(dm.daemonize(), std::system_error);
}

TEST_F(DaemonManagerTest, SetsidFailure) {
    EXPECT_CALL(*mock_sys_, fork()).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys_, setsid()).WillOnce(SetErrnoAndReturn(EPERM, -1));
    
    stc::DaemonManager dm(pidFilePath);
    EXPECT_THROW(dm.daemonize(), std::system_error);
}

TEST_F(DaemonManagerTest, ChdirFailure) {
    EXPECT_CALL(*mock_sys_, fork()).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys_, setsid()).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys_, chdir("/")).WillOnce(SetErrnoAndReturn(EACCES, -1));
    
    stc::DaemonManager dm(pidFilePath);
    EXPECT_THROW(dm.daemonize(), std::system_error);
}

TEST_F(DaemonManagerTest, WritePidChmodFailure) {
    EXPECT_CALL(*mock_sys_, chmod(_, 0644))
        .WillOnce(SetErrnoAndReturn(EPERM, -1));
    
    stc::DaemonManager dm(pidFilePath);
    dm.daemonize();
    EXPECT_THROW(dm.writePid(), std::system_error);
}

TEST_F(DaemonManagerTest, FilePermissionsCorrect) {
    stc::DaemonManager dm(pidFilePath);
    dm.daemonize();
    dm.writePid();
    
    const auto perms = fs::status(pidFilePath).permissions();
    ASSERT_EQ(perms, fs::perms::owner_read | fs::perms::owner_write | 
                    fs::perms::group_read | fs::perms::others_read);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}