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
            return EXIT_SUCCESS;
        } else {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("Fatal error: ") + e.what());
        Logger::close();
        return EXIT_FAILURE;
    }
}

/**
 * @param [in] argc Количество переданных аргументов
 * @param [in] argv Массив строк содержащих значения переданных аргументов
 * @return Логическое значение успешности выполнения разбора параметров.
 * @brief Функция разбора входящих аргументов командной строки
 * 
 * @details Функция выполняет разбор параметров, переданных из командной строки при запуске программы.
 *          Функция возвращает: false - если в результате разбора возникли ошибки, true - если разбор прошел успешно.
 */
bool ServiceController::parseArguments(int argc, char** argv) {
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            ServiceController::printHelp();
            return false;
        } else if (arg == "--reload" || arg == "-r") {
            // Параметр, при котором запускаемый экземпляр сервиса посылает запущенному (работающему) сервису сигнал о необходимости перезапуска с обновленной конфигурацией.
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
 * @brief Функция инициализации систем сервиса.abort
 * 
 * @details Предназначена для последовательной инициализации процессов сервиса.
 *          При вызове функции осуществляется инициализация обработчика системных сигналов SignalHandler, инициализация логера Logger, запуск мастер-процесса Master.
 */
void ServiceController::initialize() {
    SignalHandler::instance();

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
 * @brief Функция выполнения основного процесса ServiceController
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
 * @brief Функция вывода подсказки об использовании параметров командной строки.
 * 
 * @details Функция выполняет вывод в стандартный поток сообщения, содержащего подсказку о параметрах командной строки (help message).
 */
void ServiceController::printHelp() const {
    std::cout << "XML Filter Service\n\n"
              << "Usage:\n"
              << "  service [options]\n\n"
              << "Options:\n"
              << "  --help, -h         Show this help message\n"
              << "  --log-level        Specify logging mode (debug, info, warning, error)\n"
              << "                     If mode not specified, uses a \"info\" log mode\n"
              << "  --log-file <file>  Log messages to <file>\n" 
              << "  --log-rotate       Log rotation specified.\n" 
              << "  --log-size <bytes> Size of log file when log will rotated. \n" 
              << "                     If --log-rotate not specified, parameter value aren't use.\n"
              << "  --config <file>    Specify configuration file path\n";
}