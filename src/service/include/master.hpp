/**
 * @file master.hpp
 * @brief Управление жизненным циклом обработчика заданий (Master)
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 *
 * @details
 * Класс `Master` отвечает за:
 *  - создание и уничтожение рабочих потоков (Worker)
 *  - реакцию на сигналы перезагрузки (SIGHUP) и остановки (SIGTERM/SIGINT)
 *  - мониторинг состояния воркеров и перезапуск упавших задач
 *
 * Применяемые паттерны:
 *  - **Singleton** для глобальных логгеров и метрик
 *  - **Factory Method** для создания воркеров через AdapterFactory
 *  - **Observer** для подписки на сигналы ОС через SignalRouter
 */
#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "../include/AdapterFactory.hpp"
#include "../include/workercontainer.hpp"
#include "stc/MetricsCollector.hpp"
#include "stc/SignalRouter.hpp"

/**
 * @defgroup Core Основные компоненты сервиса
 */

/**
 * @defgroup Enums Перечисления
 */

/**
 * @defgroup Constructors Конструкторы (методы создания структур данных)
 */

/**
 * @defgroup Destructors Деструкторы (методы уничтожения структур данных)
 */

/**
 * @defgroup Control Методы управления работой основных компонент сервиса
 */

/**
 * @defgroup Monitoring Методы мониторинга состояний
 */

/**
 * @defgroup ValidationMethods Методы валидации данных
 */

/**
 * @defgroup InternalMethods Внутренние (приватные) методы классов
 */

/**
 * @class Master
 * @brief Координатор создания, перезагрузки и завершения воркеров
 *
 * @details
 * Хранит текущее состояние (`State`), контейнер воркеров (`WorkersContainer`),
 * провайдер конфигурации и регистрацию обработчиков сигналов.
 * Поддерживает следующие состояния: STOPPED, STARTING, RUNNING, RELOADING,
 * FATAL.
 *
 * @ingroup Core
 */
class Master {
 public:
  /**
   * @brief Возможные состояния мастера
   * @ingroup Enums
   *
   * @details
   * - STOPPED   — остановлен, воркеры не запущены
   * - STARTING  — инициализация и запуск воркеров
   * - RUNNING   — активная обработка
   * - RELOADING — перезагрузка конфигурации и воркеров
   * - FATAL     — фатальная ошибка, требуется ручное вмешательство
   */
  enum class State { STOPPED, STARTING, RUNNING, RELOADING, FATAL };

  /**
     * @brief Конструктор мастера
     * @ingroup Constructors
     *
     * @param[in] configProvider Функция для получения JSON-конфигурации (in)
     *
     * @details
     * Сохраняет провайдер конфигурации, регистрирует счётчики метрик:
     * "workers_created", "workers_terminated", "reload_attempts".
     *
     * @code
     Master m([](){ return ConfigManager::instance().getMergedConfig("prod");
     });
     @endcode
     *
     * @throw std::exception При ошибках регистрации метрик или сигналов
     */
  explicit Master(std::function<nlohmann::json()> configProvider);

  /**
   * @brief Деструктор мастера
   * @ingroup Destructors
   *
   * @details
   * При разрушении автоматически вызывает `stop()` для корректного завершения
   * всех воркеров и освобождения ресурсов.
   *
   * @note Деструктор не бросает исключений (noexcept).
   */
  ~Master();

  /**
     * @brief Запускает мастера и воркеры
     * @ingroup Control
     *
     * @return true  — если мастер успешно перешёл в состояние RUNNING
     *         false — если уже запущен или произошла ошибка
     *
     * @details
     * 1. Проверяет текущее состояние (должно быть STOPPED)
     * 2. Вызывает `validateConfig()` для проверки JSON
     * 3. Регистрирует наблюдателя SIGHUP для `reload()`
     * 4. Создаёт воркеры через `spawnWorkers()`
     * 5. Устанавливает состояние RUNNING и логирует кол-во воркеров
     *
     * @throw std::runtime_error При ошибках конфигурации или создания воркеров
     *
     * @code
     Master m(...);
     if (!m.start()) {
         // Обработка ошибки старта
     }
     @endcode
     */
  bool start();

  /**
   * @brief Корректно останавливает мастера и воркеры
   * @ingroup Control
   *
   * @details
   * Ставит состояние в STOPPED, вызывает `terminateWorkers()` для остановки
   * всех воркеров, увеличивает счётчик "workers_terminated" и логирует событие.
   *
   * @note Вызывается в деструкторе и при получении SIGTERM/SIGINT
   * @warning Метод не бросает исключений (noexcept).
   */
  void stop() noexcept;

  /**
   * @brief Перезагружает конфигурацию и воркеры без полного рестарта
   * @ingroup Control
   *
   * @details
   * 1. Ставит состояние RELOADING
   * 2. Валидирует новую конфигурацию через `validateConfig()`
   * 3. Создаёт временный контейнер воркеров
   * 4. Меняет текущий контейнер на новый и уничтожает старый
   * 5. Ставит состояние RUNNING и логирует завершение
   *
   * @throw std::runtime_error При ошибках валидации или создании воркеров
   *
   * @code
   * // Обрабатывается сигнал SIGHUP
   * m.reload();
   * @endcode
   */
  void reload();

  /**
   * @brief Проверяет состояние воркеров и перезапускает упавшие
   * @ingroup Monitoring
   *
   * @details
   * Проходит по каждому воркеру в `workers_`:
   * - Если `isAlive()` == false, вызывает `restart()`
   * - Увеличивает счётчик "workers_restarted"
   *
   * @note Может вызываться периодически из внешнего таймера
   * @warning Не изменяет состояние мастера
   */
  void healthCheck();

  /**
   * @brief Возвращает текущее состояние мастера
   * @ingroup Getters
   *
   * @return State Текущее состояние (`STOPPED`, `RUNNING` и т.д.)
   *
   * @note Метод всегда noexcept
   */
  State getState() const noexcept;

  /**
     * @brief Возвращает текущий размер пула воркеров
     * @ingroup Getters
     *
     * @return size_t Количество активных воркеров
     *
     * @code
     size_t count = master.getWorkerCount();
     @endcode
     */
  size_t getWorkerCount() const;

  /**
   * @brief Проверяет корректность JSON-конфигурации
   * @ingroup ValidationMethods
   *
   * @param[in] config JSON-объект конфигурации (in)
   * @throw std::runtime_error Если отсутствует массив `sources` или найден
   * неподдерживаемый тип
   *
   * @details
   * Убеждается, что `config.contains("sources")` и это массив.
   * Затем проверяет, что каждый `src["type"]` == "local".
   */
  void validateConfig(const nlohmann::json &config) const;

    void restartAllMonitoring();

 private:
  /**
   * @brief Создаёт воркеры на основе конфигурации
   * @ingroup InternalMethods
   *
   * @details
   * Вызывает `getConfig_()`, проходит по массиву `config["sources"]`,
   * создает `SourceConfig` и добавляет воркер в `workers_` если `enabled`.
   *
   * @throw std::runtime_error При ошибках создания воркера
   */
  void spawnWorkers();

  /**
   * @brief Останавливает и удаляет все воркеры
   * @ingroup InternalMethods
   *
   * @details
   * Вызывает `stopGracefully()` у каждого воркера и очищает контейнер.
   */
  void terminateWorkers();

  /**
   * @brief Контейнер активных воркеров
   *
   * @details
   * Хранит instances класса Worker, каждый из которых выполняет
   * обработку файлов по заданному источнику. Реализует пул воркеров,
   * предоставляя методы безопасного доступа и модификации через
   * WorkerContainer[1].
   *
   * @note WorkerContainer поддерживает thread-safe операции доступа
   *       к внутреннему вектору воркеров.
   * @warning Прямой доступ к внутреннему вектору не рекомендуется
   *          (используйте API WorkerContainer).
   *
   * @see WorkersContainer, Worker
   */
  WorkersContainer workers_;

  /**
   * @brief Функция-провайдер JSON-конфигурации
   *
   * @details
   * Сохраняет callback, возвращающий актуальный JSON-конфиг
   * при каждом вызове. Используется при старте, перезагрузке и
   * спавне воркеров для получения последней версии настроек.
   *
   * @note Провайдер обычно задаётся как лямбда, читающая
   *       ConfigManager::getMergedConfig(env).
   * @warning Не следует заменять после инициализации,
   *          иначе нарушится синхронизация конфигураций.
   */
  std::function<nlohmann::json()> getConfig_;

  /**
   * @brief Сигнал роутер для обработки ОС-сигналов
   *
   * @details
   * Хранит shared_ptr на объект SignalRouter, управляющий
   * регистрацией и вызовом обработчиков сигналов (SIGHUP, SIGTERM, SIGINT).
   * Используется для подписки внутри `start()` и отписки при завершении.
   *
   * @note SignalRouter — синглтон, поэтому хранится shared_ptr.
   * @warning Не освобождать объект вручную — управление временем жизни
   *          осуществляет сам SignalRouter.
   */
  std::shared_ptr<stc::SignalRouter> signalRouter_;

  /**
   * @brief Текущее состояние мастера
   *
   * @details
   * Атомарная переменная `state_` хранит текущее состояние экcемпляра Master:
   * STOPPED, STARTING, RUNNING, RELOADING или FATAL[4].
   * Обеспечивает безопасную смену состояний в многопоточном окружении.
   *
   * @note Использует compare_exchange_strong для переходов состояний.
   * @warning Не читать напрямую значение без применения memory order.
   */
  std::atomic<State> state_{State::STOPPED};

  /**
   * @brief Мьютекс для защиты конфигурации и состояния
   *
   * @details
   * Обеспечивает взаимную блокировку при изменении общего состояния
   * `baseConfig_`, `workers_` и других критических секций внутри
   * `start()`, `reload()` и `getMergedConfig()`[5].
   *
   * @note Всегда использовать вместе с std::lock_guard или std::unique_lock.
   * @warning Не допускать блокирующие вызовы внутри критической секции,
   *          чтобы избежать deadlock.
   */
  mutable std::mutex configMutex_;
};