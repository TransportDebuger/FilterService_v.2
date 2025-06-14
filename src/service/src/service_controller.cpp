#include "../include/service_controller.hpp"

#include <filesystem>
#include <stc/CompositeLogger.hpp>
#include <stc/consolelogger.hpp>
#include <stc/asyncfilelogger.hpp>
#include <stc/syncfilelogger.hpp>

using namespace std::chrono_literals;

int ServiceController::run(int argc, char** argv) {
  try {
    // Парсинг аргументов
    ArgumentParser parser;
    ParsedArgs args = parser.parse(argc, argv);
    config_path_ = args.config_path;

    // Демонизация
    if (args.daemon_mode) {
      daemon_ = std::make_unique<stc::DaemonManager>("/var/run/xmlfilter.pid");
      daemon_->daemonize();
      daemon_->writePid();
    }

    // Инициализация конфигурации
    ConfigManager::instance().initialize(config_path_);
    if (!args.overrides.empty()) {
      ConfigManager::instance().applyCliOverrides(args.overrides);
    }

    // Настройка системы
    initLogger(args);
    initialize(args);

    // Главный цикл
    stc::SignalRouter::instance().start();
    mainLoop();

    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    stc::CompositeLogger::instance().critical(e.what());
    if (daemon_) daemon_->cleanup();
    return EXIT_FAILURE;
  }
}

void ServiceController::initialize(const ParsedArgs& args) {

  // Регистрация обработчиков сигналов. За одно инициализируем синглтон SignalRouter.
  stc::SignalRouter::instance().registerHandler(SIGTERM, [this](int) { handleShutdown(); });
  stc::SignalRouter::instance().registerHandler(SIGHUP, [this](int) {
    try {
      ConfigManager::instance().reload();
      master_->reload();
    } catch (const std::exception& e) {
      stc::CompositeLogger::instance().error("Reload failed: " +
                                             std::string(e.what()));
    }
  });

  // Инициализация Master с dependency injection
  auto factory = AdapterFactoryCreator::createFactory("local");
  master_ = std::make_unique<Master>(std::move(factory));
  master_->start();
}

void ServiceController::initLogger(const ParsedArgs& args) {
    auto& logger = stc::CompositeLogger::instance();
    
    // Лямбда для безопасного создания shared_ptr из синглтона
    auto getSingletonPtr = [](auto& singleton) {
        return std::shared_ptr<std::remove_reference_t<decltype(singleton)>>(
            &singleton, 
            [](auto*){} // Пустой делитер, так как синглтон управляет своим временем жизни
        );
    };

    for (const auto& type : args.logger_types) {
        if (type == "console") {
            logger.addLogger(getSingletonPtr(stc::ConsoleLogger::instance()));
        } else if (type == "async_file") {
            auto& fileLogger = stc::AsyncFileLogger::instance();
            fileLogger.setMainLogPath("service.log");
            logger.addLogger(getSingletonPtr(fileLogger));
        } else if (type == "sync_file") {
            auto& fileLogger = stc::SyncFileLogger::instance();
            fileLogger.setMainLogPath("service.log");
            logger.addLogger(getSingletonPtr(fileLogger));
        }
    }
    
    logger.setLogLevel(stc::stringToLogLevel(args.log_level));
}

void ServiceController::mainLoop() {
  running_ = true;
  while (running_) {
    // Проверка состояния воркеров
    master_->healthCheck();

    // Обработка событий
    std::this_thread::sleep_for(500ms);
  }
}

void ServiceController::handleShutdown() {
  running_ = false;
  master_->stop();
  if (daemon_) daemon_->cleanup();
}