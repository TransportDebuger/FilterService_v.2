#pragma once
#include <csignal>
#include <pthread.h>
#include <initializer_list>

class SignalMaskGuard {
public:
    // При создании блокирует набор сигналов
    explicit SignalMaskGuard(const std::initializer_list<int>& signals) {
        sigemptyset(&mask_);
        for (int sig : signals) {
            sigaddset(&mask_, sig);
        }
        pthread_sigmask(SIG_BLOCK, &mask_, &oldMask_);
    }
    // При разрушении восстанавливает предыдущую маску
    ~SignalMaskGuard() {
        pthread_sigmask(SIG_SETMASK, &oldMask_, nullptr);
    }
    SignalMaskGuard(const SignalMaskGuard&) = delete;
    SignalMaskGuard& operator=(const SignalMaskGuard&) = delete;
private:
    sigset_t mask_;
    sigset_t oldMask_;
};