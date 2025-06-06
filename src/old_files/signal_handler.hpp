#pragma once

#include <atomic>
#include <csignal>
#include <functional>
#include <map>
#include <mutex>

class SignalHandler {
 public:
  using Callback = std::function<void(int)>;

  // Удаляем конструктор копирования и оператор присваивания
  SignalHandler(const SignalHandler&) = delete;
  SignalHandler& operator=(const SignalHandler&) = delete;

  // Получить экземпляр синглтона
  static SignalHandler& instance();

  // Регистрация обработчика сигнала
  void registerHandler(int signum, Callback callback);

  // Отмена регистрации обработчика сигнала
  void unregisterHandler(int signum);

  // Проверка флагов
  bool shouldStop() const noexcept;
  bool shouldReload() const noexcept;
  void resetFlags() noexcept;

  // Восстановление обработчиков
  void restoreHandler(int signum);
  void restoreAllHandlers();

 private:
  SignalHandler();  // Приватный конструктор для синглтона
  ~SignalHandler();

  // Внутренние методы
  static bool isValidSignal(int signum) noexcept;
  void saveOriginalHandler(int signum);
  void setSignalHandler(int signum);
  static void handleSignal(int signum) noexcept;
  void registerDefaultHandlers();

  // Данные
  std::map<int, Callback> handlers_;
  std::map<int, struct sigaction> original_handlers_;
  mutable std::mutex mutex_;
  std::atomic<bool> stop_flag_{false};
  std::atomic<bool> reload_flag_{false};
};