/**
 * @file SignalRouter.hpp
 * @brief Асинхронный маршрутизатор POSIX-сигналов с поддержкой RAII и потокобезопасностью
 *
 * @author Artem Ulyanov
 * @date Май 2025
 * @version 1.0
 *
 * @details
 * Класс SignalRouter обеспечивает безопасную и асинхронную обработку сигналов
 * в многопоточной среде с использованием signalfd(2) и epoll(7).
 *
 * Основные возможности:
 * - Регистрация нескольких обработчиков на один сигнал.
 * - Потокобезопасная модификация и вызов обработчиков.
 * - Асинхронная доставка сигналов через event loop (epoll).
 * - RAII-совместимость: автоматическая очистка ресурсов.
 * - Неблокирующая обработка с использованием SFD_NONBLOCK.
 *
 * @note
 * - Все методы, кроме конструктора, не выбрасывают исключений (но могут завершаться аварийно при ошибках программной логики).
 * - Методы start() и stop() идемпотентны при корректном использовании.
 * - Класс предполагает использование как singleton (через instance()).
 *
 * @warning
 * Не используйте этот класс в окружении, где сигналы уже обрабатываются иными способами.
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
 * @brief Потокобезопасный асинхронный маршрутизатор POSIX-сигналов (Singleton).
 *
 * Предоставляет централизованную и безопасную обработку сигналов в Linux-приложениях
 * с использованием signalfd(2) и отдельного рабочего потока. Позволяет регистрировать
 * и вызывать несколько обработчиков для одного сигнала в контексте обычного потока,
 * что исключает использование async-signal-unsafe функций в сигнальных обработчиках.
 *
 * @details
 * - Сигналы перехватываются через signalfd, преобразуя их в событие ввода-вывода.
 * - Работа организована в отдельном потоке с циклом epoll_wait(), что обеспечивает
 *   асинхронную и неблокирующую обработку.
 * - Поддержка множественных обработчиков на один сигнал — вызываются по порядку.
 * - Класс следует RAII: при уничтожении восстанавливает исходную маску сигналов.
 * - Все публичные методы являются потокобезопасными (защищены мьютексом).
 * - Обработчики вызываются без блокировки, что предотвращает взаимоблокировки.
 *
 * @note
 * - Реализован как потокобезопасный Singleton (thread-safe lazy initialization).
 * - Доступ к функциональности — через статический метод instance().
 * - Методы start() и stop() идемпотентны: повторный вызов start() игнорируется,
 *   повторный stop() безопасен.
 *
 * @warning
 * - Работает только на Linux (требует signalfd, epoll).
 * - Не может перехватывать SIGKILL и SIGSTOP — это запрещено ядром.
 * - Обработчики выполняются в рабочем потоке, поэтому долгие операции могут
 *   задерживать обработку других сигналов.
 * - Не используйте stop() внутри обработчика сигнала — это приведёт к дедлоку
 *   (поток будет ждать самого себя).
 *
 * @par Пример использования:
 * @code
 * SignalRouter::instance().registerHandler(SIGUSR1, [](int) {
 *     std::cout << "SIGUSR1 получен\n";
 * });
 * SignalRouter::instance().start();
 * 
 * // ... приложение работает ...
 * 
 * SignalRouter::instance().stop();
 * @endcode
 */
class SignalRouter {
 public:
  /// @brief Тип функции-обработчика сигнала. Принимает номер сигнала.
  using Handler = std::function<void(int)>;

  /**
   * @brief Возвращает ссылку на единственный экземпляр класса (потокобезопасный Singleton).
   *
   * Гарантирует строго одно вхождение SignalRouter в пределах процесса.
   * Инициализация экземпляра происходит при первом вызове метода (lazy initialization).
   *
   * @return Ссылка на единственный потокобезопасно инициализированный экземпляр SignalRouter.
   *
   * @note
   * - Потокобезопасность обеспечена стандартом C++11: инициализация локальной
   *   статической переменной является атомарной и исключает гонки при одновременных вызовах.
   * - Уничтожение экземпляра происходит корректно при завершении работы программы.
   * - Не поддерживается ручное управление временем жизни (например, перезапуск).
   *
   * @par Пример:
   * @code
   * auto& router = SignalRouter::instance();
   * router.registerHandler(SIGINT, [](int) { std::cout << "Получен SIGINT\n"; });
   * router.start();
   * @endcode
   */
  static SignalRouter& instance() {
    static SignalRouter router;
    return router;
  }

  SignalRouter(const SignalRouter&) = delete;
  SignalRouter& operator=(const SignalRouter&) = delete;
  SignalRouter(SignalRouter&&) = delete;
  SignalRouter& operator=(SignalRouter&&) = delete;

  void registerHandler(int signum, Handler handler);
  void unregisterHandler(int signum);

  void start();
  void stop() noexcept;

  /**
   * @brief Проверяет, активен ли обработчик сигналов в данный момент.
   *
   * Возвращает состояние рабочего цикла обработки сигналов. Используется для проверки,
   * был ли вызван метод start() и не завершён ли поток обработки.
   *
   * @return true, если обработчик запущен и активен (обработка сигналов ведётся);
   *         false, если обработчик остановлен, ещё не запускался или завершился.
   *
   * @note
   * - Потокобезопасно: внутренний флаг running_ объявлен как std::atomic<bool>,
   *   что гарантирует корректное чтение из разных потоков без дополнительной синхронизации.
   * - Значение отражает логическое состояние, а не физическое наличие потока
   *   (например, может быть true, даже если поток ещё не полностью стартовал).
   *
   * @par Пример использования:
   * @code
   * if (SignalRouter::instance().isRunning()) {
   *     SignalRouter::instance().stop();
   * }
   * @endcode
   */
  bool isRunning() const noexcept {
    return running_.load();
  }

 private:
  SignalRouter();
  
  /**
   * @brief Деструктор.
   *
   * Автоматически останавливает обработку сигналов (вызывает stop())
   * и восстанавливает исходную маску сигналов при необходимости.
   *
   * @note Вызов stop() гарантирует корректное завершение рабочего потока.
   */
  ~SignalRouter();
  void processSignals();  ///< Основной цикл обработки

  /// @brief Контейнер обработчиков: сигнал -> вектор функций-обработчиков
  std::unordered_map<int, std::vector<Handler>> handlers_;

  /// @brief Мьютекс для синхронизации доступа к handlers_ и blocked_mask_
  std::mutex handlers_mutex_;

  /// @brief Флаг, указывающий, запущен ли обработчик сигналов
  std::atomic<bool> running_{false};

  /// @brief Рабочий поток, в котором выполняется processSignals()
  std::thread worker_thread_;

  /// @brief Файловый дескриптор signalfd для чтения сигналов
  int signal_fd_ = -1;

  /// @brief Исходная маска сигналов процесса (сохраняется для корректного завершения)
  sigset_t original_mask_;

  /// @brief Текущая маска сигналов, которые блокируются и перехватываются через signalfd
  sigset_t blocked_mask_{};
};

}  // namespace stc

/**
 * @example example_usage.cpp
 * @brief Пример использования SignalRouter для обработки сигналов SIGINT и SIGHUP.
 *
 * Демонстрирует типичный сценарий:
 * - Получение экземпляра через Singleton.
 * - Регистрацию обработчиков для SIGINT и SIGHUP.
 * - Запуск асинхронной обработки сигналов.
 * - Основной цикл приложения с проверкой состояния.
 *
 * @code
 * #include <iostream>
 * #include <signal.h>
 * #include "SignalRouter.hpp"
 *
 * int main() {
 *     auto& router = SignalRouter::instance();
 *
 *     // Обработчик SIGINT: завершает работу при нажатии Ctrl+C
 *     router.registerHandler(SIGINT, [](int sig) {
 *         std::cout << "Получен сигнал завершения (SIGINT)\n";
 *         router.stop();  // Останавливаем обработку
 *     });
 *
 *     // Обработчик SIGHUP: имитация перечитывания конфигурации
 *     router.registerHandler(SIGHUP, [](int sig) {
 *         std::cout << "Перезагрузка конфигурации...\n";
 *         // Здесь может быть логика перечитывания настроек
 *     });
 *
 *     // Запускаем асинхронный цикл обработки сигналов
 *     router.start();
 *
 *     // Основной цикл приложения
 *     while (router.isRunning()) {
 *         // Здесь может быть основная логика: обработка данных, сеть и т.д.
 *         std::this_thread::sleep_for(std::chrono::milliseconds(100));
 *     }
 *
 *     std::cout << "Программа завершена.\n";
 *     return 0;
 * }
 * @endcode
 *
 * @note
 * - Метод stop() вызывается из обработчика — безопасно, так как не приводит к дедлоку
 *   (обработчик выполняется в рабочем потоке, а не в контексте сигнала).
 * - Для компиляции требуется поддержка C++17 и связывание с -lpthread.
 *
 * @see SignalRouter::instance(), SignalRouter::registerHandler(), SignalRouter::start()
 */