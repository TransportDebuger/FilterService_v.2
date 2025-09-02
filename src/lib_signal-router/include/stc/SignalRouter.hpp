/**
 * @file SignalRouter.hpp
 * @brief Асинхронный маршрутизатор POSIX-сигналов
 *
 * @author Artem Ulyanov
 * @date Май 2025
 * @version 1.0
 * @license MIT
 *
 * @details Реализует безопасную обработку сигналов через signalfd и epoll.
 * Поддерживает регистрацию множества обработчиков для одного сигнала.
 */
#pragma once

#include <sys/signalfd.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <functional>
#include <mutex>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace stc {

/**
 * @class SignalRouter
 * @brief Потокобезопасный менеджер обработки сигналов
 *
 * @note Основные возможности:
 * - Асинхронная обработка через отдельный поток
 * - Поддержка нескольких обработчиков на сигнал
 * - Гарантированное восстановление исходных обработчиков
 * - Интеграция с event loop через epoll
 *
 * @warning
 * - Только для Linux систем
 * - Не поддерживает SIGKILL и SIGSTOP
 * - Обработчики не должны выполнять не async-signal-safe операции
 */
class SignalRouter {
 public:
  using Handler = std::function<void(int)>;  ///< Тип обработчика сигналов

  /**
   * @brief Получить экземпляр SignalRouter (Singleton)
   * @return Единственный экземпляр класса
   *
   * @note Гарантирует единственность экземпляра во время работы приложения
   */
  static SignalRouter& instance() {
    static SignalRouter router;
    return router;
  }

  /**
   * @brief Зарегистрировать обработчик для сигнала
   * @param signum Номер сигнала (например, SIGINT)
   * @param handler Функция-обработчик
   * @throw std::invalid_argument При неверном номере сигнала
   * @throw std::system_error При ошибках системных вызовов
   *
   * @note Автоматически:
   * - Блокирует сигнал в главном потоке
   * - Обновляет маску signalfd
   * - Добавляет сигнал в отслеживаемые
   *
   * @code
   * router.registerHandler(SIGTERM, [](int sig) {
   *     std::cout << "Graceful shutdown requested\n";
   * });
   * @endcode
   */
  void registerHandler(int signum, Handler handler);

  /**
   * @brief Удалить все обработчики для сигнала
   * @param signum Номер сигнала
   * @throw std::invalid_argument При неверном номере сигнала
   *
   * @note Восстанавливает стандартное поведение для сигнала
   */
  void unregisterHandler(int signum);

  /**
   * @brief Запустить обработку сигналов
   * @throw std::system_error При ошибках инициализации
   *
   * @note Создает отдельный поток для мониторинга signalfd
   */
  void start();

  /**
   * @brief Остановить обработку сигналов
   * @note Гарантирует безопасное завершение рабочего потока
   */
  void stop() noexcept;

  /**
   * @brief Деструктор
   * @note Автоматически вызывает stop() и восстанавливает исходные обработчики
   */
  ~SignalRouter();

 private:
  SignalRouter();  ///< Приватный конструктор (Singleton)
  void processSignals();  ///< Основной цикл обработки

  std::unordered_map<int, std::vector<Handler>>
      handlers_;  ///< Регистр обработчиков
  std::mutex handlers_mutex_;  ///< Мьютекс для потокобезопасности
  std::atomic<bool> running_{false};  ///< Флаг работы потока
  std::thread worker_thread_;         ///< Поток обработки
  int signal_fd_ = -1;                ///< Дескриптор signalfd
  sigset_t original_mask_;   ///< Исходная маска сигналов
  sigset_t blocked_mask_{};  ///<
};

}  // namespace stc

/**
 * @example Пример использования
 * @code
 * int main() {
 *     auto& router = SignalRouter::instance();
 *
 *     router.registerHandler(SIGINT, [](int sig) {
 *         std::cout << "SIGINT received\n";
 *         SignalRouter::instance().stop();
 *     });
 *
 *     router.registerHandler(SIGHUP, [](int sig) {
 *         std::cout << "Reloading config...\n";
 *     });
 *
 *     router.start();
 *
 *     while (router.isRunning()) {
 *         // Основная логика приложения
 *     }
 * }
 * @endcode
 */