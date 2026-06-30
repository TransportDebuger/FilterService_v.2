#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "stc/SignalRouter.hpp"
#include <sys/signalfd.h>
#include <csignal>

using namespace testing;

class MockSysCalls {
public:
    virtual ~MockSysCalls() = default;
    virtual int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) = 0;
    virtual int signalfd(int fd, const sigset_t* mask, int flags) = 0;
    virtual int epoll_create1(int flags) = 0;
    virtual int epoll_ctl(int epfd, int op, int fd, epoll_event* event) = 0;
    virtual int epoll_wait(int epfd, epoll_event* events, int maxevents, int timeout) = 0;
    virtual ssize_t read(int fd, void* buf, size_t count) = 0;
};

class MockSysCallsImpl : public MockSysCalls {
public:
    MOCK_METHOD(int, sigprocmask, (int, const sigset_t*, sigset_t*), (override));
    MOCK_METHOD(int, signalfd, (int, const sigset_t*, int), (override));
    MOCK_METHOD(int, epoll_create1, (int), (override));
    MOCK_METHOD(int, epoll_ctl, (int, int, int, epoll_event*), (override));
    MOCK_METHOD(int, epoll_wait, (int, epoll_event*, int, int), (override));
    MOCK_METHOD(ssize_t, read, (int, void*, size_t), (override));
};

class stc::SignalRouterTest : public Test {
protected:
    void SetUp() override {
        mock_ = new MockSysCallsImpl();
        stc::SignalRouter::setSysCallWrapper(mock_);
    }

    void TearDown() override {
        stc::SignalRouter::resetSysCallWrapper();
        delete mock_;
    }

    MockSysCallsImpl* mock_;
};

TEST_F(SignalRouterTest, RegisterValidSignal) {
    EXPECT_CALL(*mock_, sigprocmask(_, _, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_, signalfd(_, _, _)).WillOnce(Return(1));
    
    stc::SignalRouter& router = stc::SignalRouter::instance();
    EXPECT_NO_THROW(router.registerHandler(SIGUSR1, [](int){}));
}

TEST_F(SignalRouterTest, RegisterInvalidSignal) {
    stc::SignalRouter& router = stc::SignalRouter::instance();
    EXPECT_THROW(router.registerHandler(SIGKILL, [](int){}), std::invalid_argument);
}

TEST_F(SignalRouterTest, UnregisterHandler_RemovesSignalFromMask) {
    EXPECT_CALL(*mock_, sigprocmask(_, _, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_, signalfd(_, _, _)).WillOnce(Return(1));
    EXPECT_CALL(*mock_, sigprocmask(SIG_BLOCK, _, nullptr)).WillOnce(Return(0));
    EXPECT_CALL(*mock_, signalfd(1, _, SFD_CLOEXEC | SFD_NONBLOCK)).WillOnce(Return(1));

    stc::SignalRouter& router = stc::SignalRouter::instance();
    router.registerHandler(SIGUSR1, [](int){});
    EXPECT_NO_THROW(router.unregisterHandler(SIGUSR1));
}

TEST_F(SignalRouterTest, HandlerInvocation) {
    testing::MockFunction<void(int)> mockHandler;
    signalfd_siginfo fdsi;
    fdsi.ssi_signo = SIGUSR1;

    EXPECT_CALL(*mock_, epoll_create1(EPOLL_CLOEXEC)).WillOnce(Return(2));
    EXPECT_CALL(*mock_, epoll_ctl(2, EPOLL_CTL_ADD, _, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_, read(1, _, sizeof(fdsi)))
        .WillOnce(DoAll(SetArgPointee<1>(fdsi), Return(sizeof(fdsi))));
    EXPECT_CALL(*mock_, epoll_wait(2, _, 10, 10))  // maxevents=10, timeout=10
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .RetiresOnSaturation();

    EXPECT_CALL(mockHandler, Call(SIGUSR1)).Times(1);

    stc::SignalRouter& router = stc::SignalRouter::instance();
    router.registerHandler(SIGUSR1, mockHandler.AsStdFunction());
    router.start();

    // Ждём, пока поток начнёт работу
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    router.stop();
}

TEST_F(SignalRouterTest, MultipleHandlers) {
    testing::MockFunction<void(int)> handler1, handler2;
    signalfd_siginfo fdsi;
    fdsi.ssi_signo = SIGUSR2;

    EXPECT_CALL(*mock_, epoll_create1(EPOLL_CLOEXEC)).WillOnce(Return(2));
    EXPECT_CALL(*mock_, epoll_ctl(2, EPOLL_CTL_ADD, _, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_, read(1, _, sizeof(fdsi)))
        .WillOnce(DoAll(SetArgPointee<1>(fdsi), Return(sizeof(fdsi))));
    EXPECT_CALL(*mock_, epoll_wait(2, _, 10, 10))  // maxevents=10, timeout=10
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .RetiresOnSaturation();

    EXPECT_CALL(handler1, Call(SIGUSR2)).Times(1);
    EXPECT_CALL(handler2, Call(SIGUSR2)).Times(1);

    stc::SignalRouter& router = stc::SignalRouter::instance();
    router.registerHandler(SIGUSR2, handler1.AsStdFunction());
    router.registerHandler(SIGUSR2, handler2.AsStdFunction());
    router.start();

    // Ждём, пока поток обработает сигнал
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    router.stop();
}

TEST_F(SignalRouterTest, SignalFDError) {
    EXPECT_CALL(*mock_, signalfd(_, _, _)).WillOnce(Return(-1));
    
    stc::SignalRouter& router = stc::SignalRouter::instance();
    EXPECT_THROW(router.registerHandler(SIGUSR1, [](int){}), std::system_error);
}

TEST_F(SignalRouterTest, StartAndStop_SetsRunningFlag) {
    stc::SignalRouter& router = stc::SignalRouter::instance();

    EXPECT_CALL(*mock_, epoll_create1(EPOLL_CLOEXEC)).WillOnce(Return(2));
    EXPECT_CALL(*mock_, epoll_ctl(_, _, _, _)).WillOnce(Return(0));

    EXPECT_FALSE(router.isRunning());
    router.start();
    EXPECT_TRUE(router.isRunning());
    router.stop();
    EXPECT_FALSE(router.isRunning());
}

TEST_F(SignalRouterTest, DoubleStart_AfterStop) {
    stc::SignalRouter& router = stc::SignalRouter::instance();

    EXPECT_CALL(*mock_, epoll_create1(EPOLL_CLOEXEC)).WillOnce(Return(2));
    EXPECT_CALL(*mock_, epoll_ctl(_, _, _, _)).WillOnce(Return(0));

    router.start();
    router.stop();

    EXPECT_CALL(*mock_, epoll_create1(EPOLL_CLOEXEC)).WillOnce(Return(3));
    EXPECT_CALL(*mock_, epoll_ctl(_, _, _, _)).WillOnce(Return(0));

    EXPECT_NO_THROW(router.start());  // После stop() можно перезапустить
    router.stop();
}

TEST_F(SignalRouterTest, StopWithoutStart) {
    stc::SignalRouter& router = stc::SignalRouter::instance();
    EXPECT_NO_THROW(router.stop());  // Должно быть безопасно
}