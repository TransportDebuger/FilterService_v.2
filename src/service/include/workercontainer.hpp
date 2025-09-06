/**
 * @file workercontainer.hpp
 * @brief Потокобезопасный контейнер для управления объектами Worker
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 *
 * @details
 * Класс WorkersContainer предоставляет интерфейс для хранения и
 * управления набором объектов Worker с гарантией потокобезопасного
 * доступа. Использует std::mutex для синхронизации операций чтения
 * и модификации внутреннего вектора workers_. Применяется в Master
 * для динамического запуска, остановки и перезапуска воркеров.
 *
 * @note
 * Все публичные методы обеспечивают блокировку мьютекса перед доступом
 * к данным, поэтому не требуется внешняя синхронизация.
 *
 * @warning
 * Держите время удержания мьютекса минимальным, не выполняйте внутри
 * него длительные или блокирующие операции, чтобы избежать взаимных
 * блокировок и ухудшения производительности.
 */

#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "../include/worker.hpp"

/**
 * @defgroup Core Основные компоненты сервиса
 */

/**
 * @defgroup AccessMethods Методы доступа
 */

/**
 * @defgroup Getters Методы получения данных (getters)
 */

/**
 * @defgroup Control Методы управления работой основных компонент сервиса
 */

/**
 * @class WorkersContainer
 * @brief Контейнер для безопасного хранения и обработки Worker-объектов
 *
 * @details
 * Реализует шаблон Container с потокобезопасными операциями доступа.
 * Предоставляет методы для чтения, модификации и обмена содержимым
 * в защищённых критических секциях, используя std::lock_guard и
 * std::unique_lock.
 *
 * @ingroup Core
 */
class WorkersContainer {
 public:
  /**
     * @brief Выполняет произвольную операцию с вектором воркеров
     * @ingroup AccessMethods
     *
     * @tparam Func Тип вызываемого объекта (функтор или лямбда)
     * @param[in,out] func Функтор, принимающий ссылку на вектор
     *                    std::vector<std::unique_ptr<Worker>> и
     *                    выполняющий операции чтения или записи
     *
     * @details
     * Блокирует внутренний мьютекс, вызывает func(workers_),
     * затем освобождает мьютекс. Позволяет безопасно добавлять,
     * удалять или изменять воркеры.
     *
     * @code
     // Пример использования
     container.access([](auto &workers) {
         workers.push_back(std::make_unique<MyWorker>(config));
     });
     @endcode
     *
     * @warning
     * Не выполняйте внутри func длительные операции, чтобы не
     * блокировать другие потоки.
     *
     * @throw std::bad_alloc При неудачном выделении памяти в func
     */
  template <typename Func>
  void access(Func &&func) {
    std::lock_guard lock(mutex_);
    func(workers_);
  };

  /**
   * @brief Возвращает текущий размер контейнера воркеров
   * @ingroup Getters
   *
   * @return size_t Число элементов в контейнере
   *
   * @details
   * Блокирует мьютекс, получает workers_.size(),
   * затем освобождает мьютекс.
   *
   * @code
   * size_t count = container.size();
   * @endcode
   */
  size_t size() const {
    std::lock_guard lock(mutex_);
    return workers_.size();
  };

  /**
     * @brief Атомарно заменяет содержимое двух контейнеров
     * @ingroup Control
     *
     * @param[in,out] other Контейнер, с которым будет выполнен обмен содержимым
     *
     * @details
     * Использует два std::unique_lock с defer_lock и std::lock
     * для предотвращения взаимной блокировки. После захвата
     * обоих мьютексов выполняет std::swap(workers_, other.workers_).
     *
     * @code
     // Пример обмена:
     WorkersContainer a, b;
     a.swap(b);
     @endcode
     *
     * @warning
     * Убедитесь, что никакие другие блокировки не удерживаются
     * перед вызовом swap, чтобы избежать deadlock.
     */
  void swap(WorkersContainer &other) {
    using std::swap;
    stc::CompositeLogger::instance().debug("WorkersContainer::swap() — locking mutexes");
    std::unique_lock lock1(mutex_, std::defer_lock);
    std::unique_lock lock2(other.mutex_, std::defer_lock);
    std::lock(lock1, lock2);

    swap(workers_, other.workers_);
    stc::CompositeLogger::instance().debug("WorkersContainer::swap() — swap completed");
  };

 private:
  /**
   * @brief Мьютекс для защиты доступа к внутреннему вектору
   *
   * @details
   * Защищает любые операции чтения и модификации workers_.
   *
   * @warning
   * Не выполнять длительные операции при захваченном мьютексе.
   */
  mutable std::mutex mutex_;

  /**
   * @brief Мьютекс для защиты доступа к внутреннему вектору
   *
   * @details
   * Защищает любые операции чтения и модификации workers_.
   *
   * @warning
   * Не выполнять длительные операции при захваченном мьютексе.
   */
  std::vector<std::unique_ptr<Worker>> workers_;
};