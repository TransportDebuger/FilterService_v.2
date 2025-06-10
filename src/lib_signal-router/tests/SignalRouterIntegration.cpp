#include <gtest/gtest.h>
#include <csignal>
#include <thread>
#include "stc/SignalRouter.hpp"

TEST(IntegrationTest, RealSignalHandling) {
    SignalRouter& router = SignalRouter::instance();
    bool handlerCalled = false;
    
    router.registerHandler(SIGUSR1, [&](int){
        handlerCalled = true;
    });
    
    router.start();
    
    // Отправка реального сигнала
    std::thread([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        kill(getpid(), SIGUSR1);
    }).detach();
    
    // Ожидание обработки
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    router.stop();
    
    ASSERT_TRUE(handlerCalled);
}

TEST(IntegrationTest, GracefulShutdown) {
    SignalRouter& router = SignalRouter::instance();
    router.start();
    
    std::thread([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        kill(getpid(), SIGTERM);
    }).detach();
    
    // Ожидаем завершение по SIGTERM
    ASSERT_EXIT(router.start();, KilledBySignal(SIGTERM), "");
}

TEST(IntegrationTest, ConcurrentAccess) {
    SignalRouter& router = SignalRouter::instance();
    
    auto worker = [&](int sig){
        router.registerHandler(sig, [](int){});
    };
    
    std::thread t1(worker, SIGUSR1);
    std::thread t2(worker, SIGUSR2);
    
    t1.join();
    t2.join();
    
    router.start();
    router.stop();
}