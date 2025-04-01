#include "../includes/signal_handler.hpp"
#include "../includes/logger.hpp"
#include <stdexcept>
#include <iostream>

    // // Вспомогательная функция для проверки ошибок signal()
    // void checkSignalError(int result, const char* message) {
    //     if (result == SIG_ERR) {
    //         Logger::error(message);
    //         throw std::runtime_error(message);
    //     }
    // }

SignalHandler& SignalHandler::instance() {
    static SignalHandler instance;
    return instance;
}

SignalHandler::SignalHandler() {
    registerDefaultHandlers();
}

SignalHandler::~SignalHandler() {
    restoreAllHandlers();
}

void SignalHandler::registerHandler(int signum, Callback callback) {
    if (!isValidSignal(signum)) {
        throw std::invalid_argument("Invalid signal number");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Сохраняем оригинальный обработчик при первом вызове
    if (original_handlers_.find(signum) == original_handlers_.end()) {
        saveOriginalHandler(signum);
    }

    handlers_[signum].push_back(std::move(callback));

    // Проверка наличия обработчиков
    if (!handlers_[signum].empty()) {
        setSignalHandler(signum);
    }
}

bool SignalHandler::shouldStop() const noexcept {
    return stop_flag_.load(std::memory_order_acquire);
}

bool SignalHandler::shouldReload() const noexcept {
    return reload_flag_.load(std::memory_order_acquire);
}

void SignalHandler::resetFlags() noexcept {
    stop_flag_.store(false, std::memory_order_release);
    reload_flag_.store(false, std::memory_order_release);
}

void SignalHandler::restoreHandler(int signum) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = original_handlers_.find(signum);
    if (it != original_handlers_.end()) {
        auto result = std::signal(signum, it->second);
        if (result == SIG_ERR) {
            Logger::error("Failed to restore original handler");
            throw std::runtime_error("Failed to restore original handler");
        }
        original_handlers_.erase(it);
        handlers_.erase(signum);
    }
}

void SignalHandler::restoreAllHandlers() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [signum, handler] : original_handlers_) {
        std::signal(signum, handler);
    }

    original_handlers_.clear();
    handlers_.clear();
}


bool SignalHandler::isValidSignal(int signum) noexcept {
    if (signum <= 0 || signum >= NSIG) return false;
    if (signum == SIGKILL || signum == SIGSTOP) return false;
    
    auto handler = std::signal(signum, SIG_IGN);
    if (handler == SIG_ERR) return false;
    std::signal(signum, handler); // Восстановление исходного обработчика
    
    return true;
}

void SignalHandler::saveOriginalHandler(int signum) {
    auto original = std::signal(signum, SIG_DFL);
    if (original == SIG_ERR) {
        Logger::error("Failed to get original signal handler");
        throw std::runtime_error("Failed to get original signal handler");
    }
    original_handlers_[signum] = original;
}

void SignalHandler::setSignalHandler(int signum) {
    if (std::signal(signum, &SignalHandler::handleSignal) == SIG_ERR) {
        Logger::error("Failed to set signal handler");
        throw std::runtime_error("Failed to set signal handler");
    }
}

void SignalHandler::handleSignal(int signum) noexcept {
    auto& instance = SignalHandler::instance();

    // Устанавливаем флаги
    switch (signum) {
        case SIGTERM:
        case SIGINT:
            instance.stop_flag_.store(true, std::memory_order_release);
            break;
        case SIGHUP:
            instance.reload_flag_.store(true, std::memory_order_release);
            break;
    }

    // Вызываем обработчики
    std::unique_lock<std::mutex> lock(instance.mutex_);
    auto it = instance.handlers_.find(signum);
    if (it != instance.handlers_.end()) {
        auto callbacks = it->second; // Копируем обработчики
        lock.unlock(); // Разблокируем перед вызовом

        for (const auto& callback : callbacks) {
            try {
                callback(signum);
            } catch (const std::exception& e) {
                Logger::error("Signal callback error: " + std::string(e.what()));
            } catch (...) {
                Logger::error("Unknown signal callback error");
            }
        }
    }
}

void SignalHandler::registerDefaultHandlers() {
    registerHandler(SIGTERM, [](int) {});
    registerHandler(SIGINT, [](int) {});
    registerHandler(SIGHUP, [](int) {});
}