/**
 * @file worker.hpp
 * @brief Класс обработки файлов одного источника данных в XML Filter Service
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date June, 2025
 *
 * @details
 * Worker представляет собой автономную единицу обработки, которая:
 *  - подключается к файловому хранилищу через адаптеры (локальное, SMB, FTP)
 *  - реагирует на появление новых файлов и обрабатывает их через XMLProcessor
 *  - ведет учёт метрик (обработано/ошибочных файлов)
 *  - поддерживает управление жизненным циклом: запуск, пауза, возобновление,
 * останов
 *
 * Применяемые паттерны:
 *  - **Factory** для создания адаптеров через AdapterFactory
 *  - **Strategy** для динамического выбора логики фильтрации через XMLProcessor
 *  - **Observer** для callback-уведомления о новых файлах
 *
 * @warning Должен запускаться и останавливаться из одного потока,
 *          управление состоянием потокобезопасно c помощью std::mutex.
 */

#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "../include/AdapterFactory.hpp"
#include "../include/filestorageinterface.hpp"
#include "../include/sourceconfig.hpp"
#include "stc/MetricsCollector.hpp"
#include "stc/compositelogger.hpp"

namespace fs = std::filesystem;

/**
 * @defgroup Core Основные компоненты сервиса
 */

/**
 * @defgroup Constructors Конструкторы (методы создания структур данных)
 */

/**
 * @defgroup Destructors Деструкторы (методы уничтожения структур данных)
 */

/**
 * @class Worker
 * @brief Автономная единица обработки файлов из одного источника
 * @ingroup Core
 *
 * @details
 * Worker использует AdapterFactory для подключения к различным типам
 * хранилищ и получает уведомления о новых файлах. Каждое поступление
 * файла обрабатывается через XMLProcessor, после чего файл либо перемещается
 * в processed_dir, либо в excluded_dir или bad_dir по результатам фильтрации.
 *
 * @note Все операции с состоянием (running_, paused_, processing_)
 *       защищены std::mutex и std::condition_variable.
 * @warning Некорректное завершение может привести к утечкам
 *          потоков или блокировкам.
 */
class Worker {
 public:
  /**
     * @brief Конструктор Worker на основе конфигурации источника
     * @ingroup Constructors
     *
     * @param[in] config Параметры источника данных, включая пути, фильтры и
     шаблоны
     * @throw std::runtime_error При ошибках создания адаптера (AdapterFactory)
     *
     * @details
     * Создает адаптер через AdapterFactory и регистрирует callback,
     * который вызывает processFile() при появлении новых файлов.
     *
     * @code
     SourceConfig cfg = ...;
     Worker w(cfg);
     @endcode
     *
     * @see SourceConfig
     */
  explicit Worker(const SourceConfig &config);

  /**
   * @brief Деструктор Worker
   * @ingroup Destructors
   *
   * @note Вызывает stopGracefully() для корректного завершения потока
   * @warning Исключения подавляются внутри деструктора
   */
  ~Worker();

  /**
     * @brief Запускает мониторинг и рабочий поток
     * @ingroup Lifecycle
     *
     * @throw std::runtime_error При ошибках валидации путей или подключении
     *
     * @details
     * 1. validatePaths() проверяет и создает директории
     * 2. adapter_->connect() и adapter_->startMonitoring()
     * 3. Запускает внутренний поток run()
     * 4. Устанавливает running_ = true, paused_ = false
     * 5. Собирает метрику "worker_started"
     *
     * @code
     worker.start();
     @endcode
     *
     * @note Метод потокобезопасен при одновременных вызовах
     * @warning Если параметры конфигурации некорректны, бросает исключение
     */
  void start();

  /**
   * @brief Останавливает мониторинг и поток работы
   * @ingroup Lifecycle
   *
   * @details
   * 1. Сбрасывает running_ и paused_
   * 2. Уведомляет cv_ для выхода из паузы
   * 3. adapter_->stopMonitoring() и adapter_->disconnect()
   * 4. Ждет join() у worker_thread_
   *
   * @note Метод noexcept, не бросает исключений
   */
  void stop();

  /**
     * @brief Приостанавливает обработку новых файлов
     * @ingroup Control
     *
     * @details
     * Устанавливает paused_ = true, что останавливает run()
     * на условной переменной cv_ до resume().
     *
     * @code
     worker.pause();
     @endcode
     *
     * @warning Если воркер не запущен или уже на паузе, метод ничего не делает
     */
  void pause();

  /**
   * @brief Возобновляет обработку файлов после паузы
   * @ingroup Control
   *
   * @details
   * Сбрасывает paused_ = false и уведомляет cv_.
   *
   * @code
   worker.resume();
   @endcode
   *
   * @warning Если воркер не запущен или не был на паузе, метод ничего не делает
   */
  void resume();

  /**
   * @brief Перезапускает воркер (stop и start)
   * @ingroup Control
   *
   * @details
   * Вызывает stop(), ждет 100ms и затем start().
   * Используется при критических ошибках или обновлении конфигурации.
   *
   * @code
   * worker.restart();
   * @endcode
   */
  void restart();

  /**
     * @brief Плавно останавливает воркер, дождавшись завершения обработки файла
     * @ingroup Control
     *
     * @details
     * Ждет, пока processing_ == false, затем вызывает stop().
     *
     * @code
     worker.stopGracefully();
     @endcode
     */
  void stopGracefully();

  /**
   * @brief Проверяет, запущен ли воркер
   * @ingroup StateQueries
   *
   * @return true если воркер запущен (running_), иначе false
   * @note noexcept, не бросает исключений
   */
  bool isAlive() const noexcept;

  /**
   * @brief Проверяет, приостановлен ли воркер
   * @ingroup StateQueries
   *
   * @return true если paused_ == true, иначе false
   * @note noexcept, не бросает исключений
   */
  bool isPaused() const noexcept;

  /**
   * @brief Возвращает текущую конфигурацию воркера
   * @ingroup Getters
   *
   * @return const SourceConfig& Ссылка на объект конфигурации (config_)
   * @note noexcept, не бросает исключений
   *
   * @see SourceConfig
   */
  const SourceConfig &getConfig() const noexcept { return config_; }

  void restartMonitoring();

 private:
  /**
   * @brief Основной метод потока: следит за paused_/running_ и логирует
   * статистику
   * @ingroup Internal
   *
   * @details
   * Запускается в отдельном потоке worker_thread_. В цикле:
   *  - при paused_ ждет cv_
   *  - иначе sleep(check_interval)
   *  - логирует статистику файлов за минуту
   *  - exit при running_ == false
   *
   * @warning Все исключения внутри ловятся и приводят к running_ = false
   */
  void run();

  /**
   * @brief Обрабатывает один файл по пути filePath
   * @ingroup Internal
   *
   * @param[in] filePath Путь к файлу для обработки (in)
   *
   * @details
   * 1. Проверка существования, дедупликация по SHA256
   * 2. Фильтрация через XMLProcessor если enabled
   * 3. Перемещение файла в processed/ excluded/ bad директорию
   * 4. Логирование результатов и сбор метрик
   *
   * @throw std::runtime_error При ошибках доступа к файлу или фильтрации
   */
  void processFile(const std::string &filePath);

  /**
   * @brief Проверяет доступность и создает при необходимости директории
   * @ingroup Utilities
   *
   * @throw std::runtime_error При ошибках доступа или создания директорий
   *
   * @details
   * Проходит по paths = {processed_dir, bad_dir, excluded_dir},
   * создает отсутствующие директории с помощью fs::create_directories()
   */
  void validatePaths() const;

  /**
   * @brief Вычисляет SHA256-хеш файла
   * @ingroup Utilities
   *
   * @param[in] filePath Путь к файлу (in)
   * @return std::string Шестнадцатеричная строка хеша
   *
   * @throw std::runtime_error При ошибках открытия/чтения или хеширования
   *
   * @details
   * 1. Открывает std::ifstream в бинарном режиме
   * 2. Использует OpenSSL EVP_SHA256 для вычисления
   * 3. Возвращает строку размера 64 символа
   */
  std::string getFileHash(const std::string &filePath) const;

  /**
   * @brief Формирует путь к отфильтрованному файлу
   * @ingroup Utilities
   *
   * @param[in] originalPath Исходный путь к файлу (in)
   * @return std::string Полный путь в processed_dir с шаблоном имени
   *
   * @details
   * Использует config_.getFilteredFileName() для генерации имени
   * и config_.processed_dir для директории назначения.
   */
  std::string getFilteredFilePath(const std::string &originalPath) const;

  /**
   * @brief Перемещает файл в директорию processed_dir
   * @ingroup Utilities
   *
   * @param[in] filePath      Исходный путь к файлу (in)
   * @param[in] processedPath Целевой путь для файла (in)
   *
   * @throw std::runtime_error При ошибках файловой системы
   * (fs::rename/fs::copy)
   *
   * @details
   * Если источник и назначение на разных томах, копирует и удаляет исходник;
   * иначе rename().
   */
  void moveToProcessed(const std::string &filePath,
                       const std::string &processedPath);

  /**
   * @brief Обрабатывает файл при ошибке и перемещает в bad_dir
   * @ingroup Utilities
   *
   * @param[in] filePath Путь к файлу с ошибкой (in)
   * @param[in] error    Описание ошибки (in)
   *
   * @details
   * 1. Создает bad_dir при необходимости
   * 2. Переносит или копирует файл
   * 3. Логирует предупреждение и ошибку в CompositeLogger
   */
  void handleFileError(const std::string &filePath, const std::string &error);

  /**
   * @brief Текущая конфигурация источника данных
   *
   * @details
   * Содержит все параметры источника: путь к директории, шаблон
   * имени файла, настройки фильтрации и шаблоны имен выходных файлов.
   * Инициализируется в конструкторе и остаётся неизменным на протяжении
   * всего цикла жизни Worker[2].
   *
   * @warning Не изменяйте напрямую, используйте методы applyCliOverrides()
   *          до создания Worker
   */
  SourceConfig config_;

  /**
   * @brief Уникальный тег воркера для логирования и метрик
   *
   * @details
   * Формируется как `<source_name>#<instance_id>`, где `instance_id`
   * — атомарный счётчик созданных объектов Worker. Обеспечивает удобство
   * фильтрации логов по конкретному экземпляру.
   */
  std::string workerTag_;

  /**
   * @brief Счётчик созданных экземпляров Worker
   *
   * @details
   * Статический атомарный счётчик, увеличиваемый в конструкторе
   * для генерации `workerTag_`. Гарантирует уникальность между
   * воркерами в рамках одного процесса.
   */
  static std::atomic<int> instanceCounter_;

  /**
   * @brief Адаптер для подключения к файловому хранилищу
   *
   * @details
   * Указывает на реализацию интерфейса FileStorageInterface,
   * созданную через AdapterFactory в зависимости от типа
   * источника (`local`, `smb`, `ftp` и т.д.). Предоставляет методы
   * `connect()`, `startMonitoring()`, `stopMonitoring()` и т.д.
   */
  std::unique_ptr<FileStorageInterface> adapter_;

  /**
   * @brief Флаг активности воркера
   *
   * @details
   * Атомарная булева переменная, указывающая, что воркер запущен
   * и находится в состоянии работы. Устанавливается в `true`
   * в методе `start()` и сбрасывается в `false` в `stop()`.
   */
  std::atomic<bool> running_{false};

  /**
   * @brief Флаг паузы обработки
   *
   * @details
   * Если `true`, главный цикл `run()` приостанавливается до
   * вызова `resume()`. Обеспечивает динамическую паузу обработки
   * без остановки мониторинга адаптера.
   */
  std::atomic<bool> paused_{false};

  /**
   * @brief Флаг обработки текущего файла
   *
   * @details
   * Устанавливается в `true` при входе в `processFile()`
   * и сбрасывается после завершения обработки. Предотвращает
   * конфликт повторного запуска обработки одного файла.
   */
  std::atomic<bool> processing_{false};

  /**
   * @brief Поток выполнения метода run()
   *
   * @details
   * Рабочий поток.
   * Запускается в `start()` и выполняет метод `run()`, содержащий
   * основной цикл обработки: проверка паузы, периодическое логирование
   * статистики и ожидание новых файлов через callback адаптера.
   */
  std::thread worker_thread_;

  /**
   * @brief Мьютекс для синхронизации состояний
   *
   * @details
   * Защищает доступ и изменение булевых флагов `running_`, `paused_`
   * и условной переменной `cv_`. Используется в `run()`, `pause()`,
   * `resume()` и `stop()`.
   */
  mutable std::mutex state_mutex_;

  /**
   * @brief Условная переменная для управления паузой
   *
   * @details
   * Позволяет потоку `run()` блокироваться при `paused_ == true`
   * и возобновлять работу при `cv_.notify_all()` из `resume()`
   * или `stop()`.
   */
  std::condition_variable cv_;

  /**
   * @brief Время последнего старта цикла `run()`
   *
   * @details
   * Используется для периодического логирования статистики
   * о количестве обработанных и не обработанных файлов
   * за интервал в одну минуту.
   */
  std::chrono::steady_clock::time_point start_time_;

  /**
   * @brief Счетчик успешно обработанных файлов
   *
   * @details
   * Атомарный счётчик, увеличиваемый при успешном завершении
   * `processFile()`. Используется для метрик через MetricsCollector.
   */
  std::atomic<size_t> files_processed_{0};

  /**
   * @brief Счетчик файлов, обработка которых завершилась ошибкой
   *
   * @details
   * Атомарный счётчик, увеличиваемый в блоке `catch` метода
   * `processFile()` при возникновении исключений. Используется
   * для генерации метрики о неудачных операциях.
   */
  std::atomic<size_t> files_failed_{0};
};