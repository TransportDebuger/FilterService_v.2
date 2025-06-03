#pragma once
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include <atomic>
#include <functional>
#include <system_error>
#include <thread>
#include <unordered_map>

/**
 * @class SignalRouter
 * @brief Асинхронный маршрутизатор сигналов с использованием signalfd и epoll
 *
 * @note Особенности:
 * - Использует signalfd для асинхронного получения сигналов
 * - Обработка в отдельном потоке с очередью событий
 * - Гарантированная потокобезопасность
 * - Поддержка эпиллинга для интеграции в event loop
 */
class SignalRouter {
 public:
  SignalRouter();
  ~SignalRouter();

  /**
   * @brief Регистрирует обработчик для сигнала
   * @param signum Номер сигнала
   * @param handler Функция-обработчик
   * @throw std::system_error При ошибках системных вызовов
   */
  void registerHandler(int signum, std::function<void()> handler);

  /**
   * @brief Запускает обработку сигналов в отдельном потоке
   */
  void start();

  /**
   * @brief Останавливает обработку сигналов
   */
  void stop();

 private:
  void setupSignalFD();
  void processEvents();
  void updateSignalMask();

  int epoll_fd_;
  int signal_fd_;
  std::atomic<bool> running_{false};
  std::thread worker_thread_;
  std::unordered_map<int, std::function<void()>> handlers_;
  sigset_t signal_mask_;
  mutable std::mutex mutex_;
};