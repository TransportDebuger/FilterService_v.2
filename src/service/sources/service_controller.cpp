/**
 * @file service_controller.cpp
 * @author Artem Ulyanov
 * @date March 2025
 * @brief Реализация основного контроллера сервиса фильтрации XML данных
 * 
 * @details Содержит реализацию:
 * - Обработки аргументов командной строки
 * - Инициализации сервиса и мастер-процесса
 * - Главного цикла работы сервиса
 * - Механизма перезагрузки через сигналы
 * 
 * @section Основные_функции
 * - run(): Управляет всем жизненным циклом приложения
 * - parseArguments(): Обрабатывает параметры CLI
 * - sendReloadSignal(): Отправляет SIGHUP работающему процессу
 * - initialize(): Настраивает обработчики сигналов и мастер-процесс
 * - mainLoop(): Обеспечивает непрерывную работу сервиса
 * 
 * @subsection Особенности_реализации
 * - Использует PID-файл (/var/run/service.pid) для управления процессами
 * - Автоматически удаляет PID-файл при завершении
 * - Поддерживает graceful shutdown при получении SIGTERM/SIGINT
 * - Реализует атомарную перезагрузку конфигурации
 * 
 * @note Требования:
 * - Для работы --reload необходим PID-файл
 * - Конфигурация по умолчанию должна быть доступна по пути ./config.json
 * 
 * @warning При параллельном запуске нескольких экземпляров возможны конфликты:
 * - Перезапись PID-файла
 * - Конфликты доступа к ресурсам
 * 
 * @see ServiceController
 * @see Logger
 * @see Master
 * @see SignalHandler
 */

#include "../includes/service_controller.hpp"
#include "../includes/signal_handler.hpp"
//#include "../includes/daemonizer.hpp"
#include <thread>

/**
 * @public
 * @param [in] argc Количество переданных аргументов
 * @param [in] argv Массив строк содержащих значения переданных аргументов
 * @return Возвращаемый код выполнения процесса. 0 - если успешно.
 * @brief Функция запуска выполнения основного процесса
 */
int ServiceController::run(int argc, char** argv) {
    try {
        if (parseArguments(argc, argv)) {
            initialize();
            mainLoop();
            Logger::close();
            std::remove("/var/run/service.pid");
            return EXIT_SUCCESS;
        } else {
            Logger::fatal("Fatal error: Can't parse CLI parameters");
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        Logger::fatal(std::string("Fatal error: ") + e.what());
        Logger::close();
        return EXIT_FAILURE;
    }
}

/**
     * @brief Парсит аргументы командной строки
     * @param [in] argc Количество аргументов (int)
     * @param [in] argv Массив аргументов (char*)
     * @return true - парсинг успешен, false - ошибка/необходимость выхода
     * 
     * @details Поддерживаемые аргументы:
     * - --help/-h          Вывод справки
     * - --reload/-r        Перезагрузка конфигурации
     * - --log-level LEVEL  Уровень логирования
     * - --config FILE      Путь к конфигурации
     * - --log-file FILE    Файл для логирования
     */
bool ServiceController::parseArguments(int argc, char** argv) {
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            ServiceController::printHelp();
            return false;
        } else if (arg == "--reload" || arg == "-r") {
            if (sendReloadSignal()) {
                std::cout << "Reload signal sent" << std::endl;
                exit(EXIT_SUCCESS);
            } else {
                Logger::fatal("Failed to send reload signal");
                return false;
            }
        } else if (arg == "--log-level" && i + 1 < argc) {
            Logger::setLevel(Logger::strToLogLevel(argv[++i]));
        } else if (arg == "--config" && i + 1 < argc) {
            config_path_ = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            Logger::setLogPath(argv[++i]);
        } else if (arg == "--log-rotate") {
            Logger::setLogRotation(true);
        } else if (arg == "--log-size" && i + 1 < argc) {
            Logger::setLogSize(atoi(argv[++i]));
        } else {
            std::cout << "Unknown argument: " + arg << std::endl;
            printHelp();
            return false;
        }
    }

    return true;
}

/**
     * @brief Отправляет сигнал перезагрузки работающему процессу
     * @return true - сигнал отправлен, false - ошибка
     * 
     * @details Читает PID из /var/run/service.pid.
     *          Логирует ошибки при:
     * - Отсутствии PID-файла
     * - Невалидном PID
     * - Ошибках отправки сигнала
     */
bool ServiceController::sendReloadSignal() {
    std::ifstream pidFile("/var/run/service.pid");
    if (!pidFile) {
        Logger::error("PID file not found");
        return false;
    }

    pid_t pid;
    if (!(pidFile >> pid)) {
        Logger::error("Invalid PID in file");
        return false;
    }

    if (kill(pid, SIGHUP) != 0) {
        if (errno == EPERM) {
            Logger::error("Permission denied to send signal to PID: " + std::to_string(pid));
        }
        Logger::error("Failed to send SIGHUP to PID: " + std::to_string(pid));
        return false;
    }
    return true;
}

/**
     * @brief Инициализирует компоненты сервиса
     * 
     * @details Создает PID-файл, настраивает:
     * - Обработчики сигналов (SignalHandler)
     * - Мастер-процесс (Master)
     * - Логгер (Logger)
     * @throw std::runtime_error При ошибке запуска Master
     */
void ServiceController::initialize() {
    SignalHandler::instance();
    std::ofstream pidFile("/var/run/service.pid");
    pidFile << getpid();
    //Подумать, нужна ли реализация демонизации сервиса.
    // if (run_as_daemon_) {
    //      Daemonizer::daemonize();
    // }

    if (!master_.start(config_path_)) {
         throw std::runtime_error("Failed to start master process");
    }
    
    Logger::init();
}

/**
     * @brief Главный цикл работы сервиса
     * 
     * @details Обрабатывает:
     * - Сигналы остановки (SIGTERM/SIGINT)
     * - Запросы на перезагрузку конфигурации (SIGHUP)
     * - Периодические проверки состояния (раз в секунду)
     */
void ServiceController::mainLoop() {
    while (!SignalHandler::instance().shouldStop()) {
        if (SignalHandler::instance().shouldReload()) {
            if (master_.getState() == Master::State::RUNNING) {
                try {
                    master_.reload(); // Перезагрузка конфигурации
                    SignalHandler::instance().resetFlags();
                    Logger::info("Service reloaded successfully");
                } catch (const std::exception& e) {
                    Logger::error("Service reload failed: " + std::string(e.what()));
                }
            } else {
                Logger::warn("Reload attempted while not in RUNNING state");
                SignalHandler::instance().resetFlags();
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    master_.stop(); // Корректная остановка при получении SIGTERM/SIGINT
}

/**
     * @brief Выводит справку по аргументам командной строки
     * 
     * @details Формат вывода:
     * - Описание сервиса
     * - Список поддерживаемых аргументов
     * - Примеры использования
     */
void ServiceController::printHelp() const {
    std::cout << "XML Filter Service\n\n"
              << "Usage:\n"
              << "  service [options]\n\n"
              << "Options:\n"
              << "  --help, -h          Show this help message\n"
              << "  --reload, -r        Send reload signal to running service\n"
              << "  --log-level         Specify logging mode (debug, info, warning, error)\n"
              << "                      If mode not specified, uses a \"info\" log mode\n"
              << "  --log-file <file>   Log messages to <file>\n" 
              << "  --log-rotate        Log rotation specified.\n" 
              << "  --log-size <bytes>  Size of log file when log will rotated. \n" 
              << "                      If --log-rotate not specified, parameter value aren't use.\n"
              << "  --config <file>     Specify configuration file path\n";
}