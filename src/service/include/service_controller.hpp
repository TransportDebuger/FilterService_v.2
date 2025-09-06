/**
 * @file service_controller.hpp
 * @brief Класс управления жизненным циклом сервиса
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 *
 * @details
 * ServiceController объединяет разбор аргументов, демонизацию процесса,
 * инициализацию конфигурации, логирования и главного цикла работы сервиса.
 * Использует паттерны Singleton для ConfigManager, Strategy для ArgumentParser
 * и Observer для SignalRouter.
 *
 * @note Не потокобезопасен при параллельном вызове run()
 * @warning Ошибки инициализации завершают процесс с кодом EXIT_FAILURE
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "../include/argumentparser.hpp"
#include "../include/configmanager.hpp"
#include "../include/master.hpp"
#include "../include/pid_file_manager.hpp"
#include "../include/config_reload_transaction.hpp"
#include "stc/DaemonManager.hpp"
#include "stc/SignalRouter.hpp"

/**
 * @defgroup Core Основные компоненты сервиса
 */

/**
 * @defgroup MainAPI Основные методы
 */

/**
 * @defgroup SignalHandlers Методы обработки системных сигналов
 */

/**
 * @defgroup Initialization Методы инициализации структур данных
 */

/**
 * @class ServiceController
 * @brief Управляет запуском, конфигурацией и жизненным циклом сервиса
 * @ingroup Core
 *
 * @details
 * Реализует следующие этапы:
 * 1. Парсинг и валидация CLI аргументов
 * 2. Демонизация (опционально)
 * 3. Загрузка и применение конфигурации
 * 4. Инициализация логгера
 * 5. Запуск SignalRouter и главного цикла обработки
 *
 * @see ArgumentParser, ConfigManager, Master, FilterListManager
 */
class ServiceController {
 public:
  /**
   * @brief Основная точка входа сервиса
   * @ingroup MainAPI
   *
   * @param[in] argc Количество аргументов командной строки
   * @param[in] argv Массив аргументов (имя программы и параметры)
   * @return int Код завершения (EXIT_SUCCESS или EXIT_FAILURE)
   * @throw std::runtime_error При ошибках загрузки конфигурации или демонизации
   *
   * @details
   *  - Вызывает ArgumentParser::parse() для разбора CLI
   *  - При необходимости демонизирует процесс через DaemonManager
   *  - Загружает конфиг через ConfigManager::initialize()
   *  - Применяет CLI-переопределения
   *  - Инициализирует логгер и FilterListManager
   *  - Запускает SignalRouter, mainLoop и master
   *
   * @note Запуск из main() должен быть единственным вызовом run()
   * @warning Неперехваченные исключения переводят процесс в EXIT_FAILURE
   *
   * @code
   int main(int argc, char** argv) {
       ServiceController svc;
       return svc.run(argc, argv);
   }
   @endcode
   */
  int run(int argc, char **argv);

 private:
  /**
      * @brief Настраивает SignalRouter и Master
      * @ingroup Initialization
      *
      * @param[in] args Результаты парсинга CLI (ParsedArgs)
      *
      * @details
      *  - Регистрирует обработчики SIGTERM, SIGINT (graceful shutdown)
      *  - Регистрирует SIGHUP для reload-конфигурации
      *  - Создаёт и запускает Master с лямбдой для получения конфига
      *
      * @note Вызывается внутри run() после initLogger()
      * @warning Не вызывать до загрузки конфигурации
      *
      * @code
      ServiceController svc;
      ParsedArgs args = parser.parse(argc, argv);
      svc.initialize(args);
      @endcode
      */
  void initialize(const ParsedArgs &args);

  /**
     * @brief Инициализирует логи по конфигу или CLI
     * @ingroup Initialization
     *
     * @param[in] args ParsedArgs, содержащий параметры логирования (in)
     *
     * @details
     *  - Если args.use_cli_logging == true, создаёт логгеры из CLI
     *  - Иначе читает секцию "logging" из ConfigManager::getMergedConfig()
     *  - Для каждого типа ("console","async_file","sync_file")
     *    устанавливает path и уровень через setLogLevel()
     *
     * @note Вызывать до создания FilterListManager
     * @warning Уровень из CLI (--log-level) имеет приоритет
     *
     * @code
     ServiceController svc;
     ParsedArgs args = parser.parse(argc, argv);
     svc.initLogger(args);
     @endcode
     */
  void initLogger(const ParsedArgs &args);

  /**
   * @brief Главный цикл работы сервиса
   * @ingroup Core
   *
   * @details
   *  - Ожидание cv_ с таймаутом 500ms
   *  - Вызывает master_->healthCheck() каждую итерацию
   *  - Прерывается по signalShutdown()
   *
   * @note Использует condition_variable для прерываемого ожидания
   * @warning Не вызывать из других потоков напрямую
   */
  void mainLoop();

  /**
   * @brief Обработчик сигналов SIGTERM и SIGINT
   * @ingroup SignalHandlers
   *
   * @details
   *  - Сбрасывает флаг running_ (false)
   *  - Пробуждает mainLoop() через cv_.notify_one()
   *  - Останавливает master_ и DaemonManager
   *  - Вызывает cleanup у DaemonManager и SignalRouter::stop()
   *
   * @note Регистрируется в initialize()
   * @warning Может быть вызван асинхронно сигналом ОС
   */
  void handleShutdown();

  void reloadWorkers(const ParsedArgs &args);

  void printHelp();
void printVersion();

std::unique_ptr<PidFileManager> pidFileMgr_;

  /**
     * @brief Экземпляр основного менеджера обработки задач
     *
     * @details
     * Хранит указатель на объект `Master`, отвечающий за:
     *  - Запуск рабочих потоков для обработки файлов
     *  - Мониторинг состояния воркеров
     *  - Координацию операций фильтрации XML
     *
     * Инициализируется в методе `initialize()` через dependency injection:
     * @code
     master_ = std::make_unique<Master>([&args](){
         return ConfigManager::instance().getMergedConfig(args.environment);
     });
     master_->start();
     @endcode
     *
     * @note Использует шаблон «Фабрика» для передачи конфигурации
     * @warning Не создавать напрямую до загрузки конфигурации
     */
  std::unique_ptr<Master> master_;

  /**
   * @brief Менеджер демонизации процесса
   *
   * @details
   * Хранит указатель на объект `DaemonManager`, выполняющий:
   *  - Отсоединение процесса от терминала
   *  - Запись PID-файла в заданное место (например, `/var/run/xmlfilter.pid`)
   *  - Обеспечение работы в непрерывном фоновом режиме
   *
   * Инициализируется при наличии флага `--daemon`:
   * @code
   * daemon_ = std::make_unique<DaemonManager>("/var/run/xmlfilter.pid");
   * daemon_->daemonize();
   * daemon_->writePid();
   * @endcode
   *
   * @note Демонизация выполняется **до** инициализации ConfigManager
   * @warning Вызывать только один раз в процессе запуска
   */
  std::unique_ptr<stc::DaemonManager> daemon_;

  /**
   * @brief Путь к файлу конфигурации
   *
   * @details
   * Строковый путь (std::string) к JSON-файлу конфигурации, переданный
   * через параметр `--config-file` или используемый по умолчанию
   * `"config.json"`.
   *
   * Используется методами:
   *  - `ConfigManager::instance().initialize(config_path_)`
   *  - При перезагрузке в сигнале SIGHUP: `ConfigManager::instance().reload()`
   *
   * @note По умолчанию `"config.json"` в текущем рабочем каталоге
   * @warning Не менять после инициализации — приводит к несоответствию
   * конфигурации
   */
  std::string config_path_ = "config.json";

  /**
   * @brief Флаг состояния основного цикла обработки
   *
   * @details
   * Атомарная булева переменная (`std::atomic<bool>`) указывает,
   * должен ли ещё выполняться метод `mainLoop()`. Устанавливается:
   *  - `true` при старте mainLoop()
   *  - `false` в обработчике `handleShutdown()` при получении SIGTERM/SIGINT
   *
   * Гарантирует потокобезопасное чтение и запись без использования внешнего
   * мьютекса.
   *
   * @see std::atomic<bool>
   */
  std::atomic<bool> running_{false};

  /**
     * @brief Мьютекс для синхронизации доступа к `cv_` и состоянию `running_`
     *
     * @details
     * Используется вместе с `std::condition_variable` для:
     *  - Блокировки потока в `mainLoop()`
     *  - Пробуждения потока при вызове `handleShutdown()`
     *
     * Оборачивается в `std::unique_lock<std::mutex>` при вызове:
     * @code
     std::unique_lock<std::mutex> lock(mtx_);
     cv_.wait_for(lock, milliseconds(500), [&]{ return !running_; });
     @endcode
     *
     * @see std::mutex
     */
  std::mutex mtx_;

  /**
     * @brief Условная переменная для прерывания ожидания в `mainLoop()`
     *
     * @details
     * Используется для блокировки потока при отсутствии работы:
     * @code
     cv_.wait_for(lock, std::chrono::milliseconds(500),
                [this]{ return !running_; });
     @endcode
     * В методе `handleShutdown()` вызывается `cv_.notify_one()`,
     * чтобы разблокировать `mainLoop()` и корректно завершить работу.
     *
     * @see std::condition_variable
     */
  std::condition_variable cv_;

  std::atomic<bool> shutdown_requested_{false};
};