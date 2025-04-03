#include "../includes/service_controller.hpp"
#include "../includes/signal_handler.hpp"
//#include "../includes/daemonizer.hpp"
#include <thread>

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

bool ServiceController::parseArguments(int argc, char** argv) {
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            ServiceController::printHelp();
            return false;
        } else if (arg == "--log-level" && i + 1 < argc) {
            Logger::setLevel(Logger::strToLogLevel(argv[++i]));
        } else if (arg == "--config" && i + 1 < argc) {
            config_path_ = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            Logger::setLogPath(argv[++i]);
        } else {
            std::cout << "Unknown argument: " + arg << std::endl;
            printHelp();
            return false;
        }
    }

    return true;
}

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

void ServiceController::printHelp() const {
    std::cout << "XML Filter Service\n\n"
              << "Usage:\n"
              << "  service [options]\n\n"
              << "Options:\n"
              << "  --help, -h      Show this help message\n"
              << "  --log-level     Run with specified logging mode (debug, info, warning, error)\n"
              << "                  If mode not specified, uses a info log mode\n"
              << "  --config <path> Specify configuration file path\n";
}