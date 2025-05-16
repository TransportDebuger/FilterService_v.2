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

#include <thread>
#include <string_view>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

#include "../include/service_controller.hpp"
#include "stc/compositelogger.hpp"
#include "stc/asyncfilelogger.hpp"
#include "stc/syncfilelogger.hpp"
#include "stc/consolelogger.hpp"
#include "../include/configmanager.hpp"

#include "../include/signal_handler.hpp"
//#include "../includes/daemonizer.hpp"

#define ENV_TYPE "development"
#define PID_FILE_NAME "/var/run/filterservice.pid"

stc::LogLevel parseLogLevel(const std::string& levelStr) {
    using namespace stc;
    if (levelStr == "debug") return LogLevel::LOG_DEBUG;
    if (levelStr == "info") return LogLevel::LOG_INFO;
    if (levelStr == "warning") return LogLevel::LOG_WARNING;
    if (levelStr == "error") return LogLevel::LOG_ERROR;
    if (levelStr == "critical") return LogLevel::LOG_CRITICAL;
    return LogLevel::LOG_INFO;
}

int ServiceController::run(int argc, char** argv) {
  try {
    if (parseArguments(argc, argv)) {
      ConfigManager::instance().loadFromFile(config_path_); //Читаем конфигурационный файл сервиса.
      initLogger();
      initialize();
      mainLoop();
      std::remove(PID_FILE_NAME);
      return EXIT_SUCCESS;
    } else {
      std::runtime_error("Fatal error: Can't parse CLI parameters");
      return EXIT_FAILURE;
    }
  } catch (const std::exception& e) {
    std::cerr << "fatal" << e.what();
    // Logger::fatal(std::string("Fatal error: ") + e.what());
    // Logger::close();
    return EXIT_FAILURE;
  }
}

bool ServiceController::parseArguments(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.starts_with("--log-type")) {
      std::string value;
      size_t eqPos = arg.find('=');
      if (eqPos != std::string::npos) {
        value = arg.substr(eqPos + 1);
      } else if (i + 1 < argc) {
        value = argv[++i];
      } else {
        std::cerr << "--log-type requires a value" << std::endl;
        return false;
      }
      size_t pos = 0;
      while ((pos = value.find(',')) != std::string::npos) {
        loggerTypes_.push_back(value.substr(0, pos));
        value.erase(0, pos + 1);
      }
      if (!value.empty()) loggerTypes_.push_back(value);
    } else if (arg == "--help" || arg == "-h") {
      ServiceController::printHelp();
      return false;
    } else if (arg == "--config" && i + 1 < argc) {
      config_path_ = argv[++i];
    } else if (arg == "--log-level" && i + 1 < argc) {
      cliLogLevel_ = argv[++i];
    } else if (arg == "--reload" || arg == "-r") {
      if (sendReloadSignal()) {
        stc::ConsoleLogger::instance().info("Reload signal sent");
        exit(EXIT_SUCCESS);
      } else {
        exit(EXIT_FAILURE);
      }
    } else {
      std::cout << "Unknown argument: " + arg << std::endl;
      printHelp();
      return false;
    }
  }
  
  // for (int i = 1; i < argc; ++i) {
  //   std::string arg = argv[i];

  //   if (arg == "--help" || arg == "-h") {
  //     ServiceController::printHelp();
  //     return false;
  //   } else if (arg == "--reload" || arg == "-r") {
  //     if (sendReloadSignal()) {
  //       ConsoleLogger::info("Reload signal sent");
  //       exit(EXIT_SUCCESS);
  //     } else {
  //       ConsoleLogger::fatal("Failed to send reload signal");
  //       return false;
  //     }
  //   } else if (arg == "--log-level" && i + 1 < argc) {
  //     ConsoleLogger::setLevel(Logger::strToLogLevel(argv[++i]));
  //     //Добавить файловый логгер.
  //   } else if (arg == "--config" && i + 1 < argc) {
  //     config_path_ = argv[++i];
  //   } else if (arg == "--log-file" && i + 1 < argc) {
  //     //Logger::setLogPath(argv[++i]);
  //   } else if (arg == "--log-rotate") {
  //     //Logger::setLogRotation(true);
  //   } else if (arg == "--log-size" && i + 1 < argc) {
  //     //Logger::setLogSize(atoi(argv[++i]));
  //   } else {
  //     std::cout << "Unknown argument: " + arg << std::endl;
  //     printHelp();
  //     return false;
  //   }
  // }

  return true;
}

void ServiceController::initLogger() {
    using namespace stc;
    CompositeLogger& compositeLogger = CompositeLogger::instance();

    // Получаем конфиг и секцию логирования
    ConfigManager& config = ConfigManager::instance();
    auto loggingConfig = config.getLoggingConfig(ENV_TYPE);

    // Получаем общий уровень логирования из секции logging (если есть)
    std::optional<std::string> globalLevel;
    if (loggingConfig.is_object() && loggingConfig.contains("level")) {
        globalLevel = loggingConfig["level"].get<std::string>();
    }

    // Определяем, какие логгеры использовать
    std::vector<std::string> loggerTypesToUse;
if (!loggerTypes_.empty()) {
    loggerTypesToUse = loggerTypes_;
} else {
        // Собираем типы из конфига
        for (const auto& loggerConf : loggingConfig) {
            if (loggerConf.contains("type")) {
                loggerTypesToUse.push_back(loggerConf["type"].get<std::string>());
            }
        }
    }

    // Для каждого типа логгера создаём и настраиваем экземпляр
    for (const auto& type : loggerTypesToUse) {
        std::optional<std::string> loggerLevel;
        std::string logFilePath;

        // Находим соответствующий loggerConfig (если есть)
        nlohmann::json loggerConfig;
        for (const auto& conf : loggingConfig) {
            if (conf.contains("type") && conf["type"] == type) {
                loggerConfig = conf;
                if (conf.contains("level"))
                    loggerLevel = conf["level"].get<std::string>();
                if (conf.contains("file"))
                    logFilePath = conf["file"].get<std::string>();
                break;
            }
        }

        // Определяем итоговый уровень логирования
        LogLevel level = LogLevel::LOG_INFO;
        if (cliLogLevel_) {
            level = parseLogLevel(*cliLogLevel_);
        } else if (loggerLevel) {
            level = parseLogLevel(*loggerLevel);
        } else if (globalLevel) {
            level = parseLogLevel(*globalLevel);
        }

        if (type == "console") {
            ConsoleLogger::instance().setLogLevel(level);
            //logger->setLogLevel(level);
            //compositeLogger.addLogger(logger);
        } 
        // else if (type == "async_file") {
        //     auto logger = std::make_shared<AsyncFileLogger>(AsyncFileLogger::instance());
        //     logger->setLogLevel(level);
        //     if (!logFilePath.empty())
        //         logger->setMainLogPath(logFilePath);
        //     compositeLogger.addLogger(logger);
        // } else if (type == "sync_file") {
        //     auto logger = std::make_shared<SyncFileLogger>(SyncFileLogger::instance());
        //     logger->setLogLevel(level);
        //     if (!logFilePath.empty())
        //         logger->setMainLogPath(logFilePath);
        //     stc::CompositeLogger::instance().addLogger(logger);
        // }
        // Добавьте другие типы логгеров по необходимости (необходимо дополнение библиотеки liblogger)
    }

    // Инициализация CompositeLogger на самом высоком уровне (например, debug)
    // LogLevel initLevel = LogLevel::LOG_INFO;
    // if (cliLogLevel_) {
    //     initLevel = parseLogLevel(*cliLogLevel_);
    // } else if (globalLevel) {
    //     initLevel = parseLogLevel(*globalLevel);
    // }
    // compositeLogger.init(initLevel);
    ConsoleLogger::instance().info("Logger initialized");
    ConsoleLogger::instance().debug("Logging level sets to: " + leveltoString(ConsoleLogger::instance().getLogLevel()));
}

bool ServiceController::sendReloadSignal() {
  std::ifstream pidFile(PID_FILE_NAME);
  if (!pidFile) {
    stc::ConsoleLogger::instance().error("PID file not found");
    //stc::CompositeLogger::instance().error("PID file not found");
    return false;
  }

  pid_t pid;
  if (!(pidFile >> pid)) {
    stc::ConsoleLogger::instance().error("Invalid PID in file");
    //stc::CompositeLogger::instance().error("Invalid PID in file");
    return false;
  }

  if (kill(pid, SIGHUP) != 0) {
    if (errno == EPERM) {
      stc::ConsoleLogger::instance().error("Permission denied to send signal to PID: " +
                    std::to_string(pid));
      // stc::CompositeLogger::instance().error("Permission denied to send signal to PID: " +
      //               std::to_string(pid));
    }
    stc::ConsoleLogger::instance().error("Failed to send SIGHUP to PID: " + std::to_string(pid));
    return false;
  }
  return true;
}

void ServiceController::initialize() {
  stc::ConsoleLogger::instance().info("Service initialization");
  SignalHandler::instance();
  std::ofstream pidFile(PID_FILE_NAME);
  pidFile << getpid();
  if (chmod(PID_FILE_NAME, 0600) != 0) {
    stc::ConsoleLogger::instance().warning("Failed to set PID file permissions");
  }
  //Подумать, нужна ли реализация демонизации сервиса.
  // if (run_as_daemon_) {
  //      Daemonizer::daemonize();
  // }

  // if (!master_.start(config_path_)) {
  //   throw std::runtime_error("Failed to start master process");
  // }
}

void ServiceController::mainLoop() {
  while (!SignalHandler::instance().shouldStop()) {
    if (SignalHandler::instance().shouldReload()) {
      // if (master_.getState() == Master::State::RUNNING) {
      //   try {
      //     master_.reload();  // Перезагрузка конфигурации
      //     SignalHandler::instance().resetFlags();
      //    // Logger::info("Service reloaded successfully");
      //   } catch (const std::exception& e) {
      //     //Logger::error("Service reload failed: " + std::string(e.what()));
      //   }
      // } else {
      //   //Logger::warn("Reload attempted while not in RUNNING state");
      //   SignalHandler::instance().resetFlags();
      // }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  //master_.stop();  // Корректная остановка при получении SIGTERM/SIGINT
}

void ServiceController::printHelp() const {
  std::cout
      << "XML Filter Service\n\n"
      << "Usage:\n"
      << "  service [options]\n\n"
      << "General options:\n"
      << "  --help, -h          Show this help message\n"
      << "  --reload, -r        Send reload signal to running service\n"
      << "\nConfig options\n"
      << "  --config <file>     Specify configuration file path\n"
      << "\nLogging options:\n"
      << "  --log-type=TYPES    Specify logger types (comma-separated)\n"
      << "                      Available types: console, async_file, sync_file\n"
      << "  --log-level         Specify logging mode (debug, info, warning, "
         "error)\n"
      << "                      If mode not specified, uses a \"info\" log "
         "mode\n"
      << "  --log-file <file>   Log messages to <file>\n"
      << "  --log-rotate        Log rotation specified.\n"
      << "  --log-size <bytes>  Size of log file when log will rotated. \n"
      << "                      If --log-rotate not specified, parameter value "
         "aren't use.\n";
}