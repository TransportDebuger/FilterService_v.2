/**
 * @file service_controller.cpp
 * @brief Реализация методов ServiceController
 *
 * @details
 * Методы обеспечивают полное управление жизненным циклом сервиса:
 *  - run(): инициализация и запуск
 *  - initialize(): настройка сигналов и Master
 *  - initLogger(): конфигурация логирования
 *  - mainLoop(): работа в цикле с healthCheck()
 *  - handleShutdown(): завершение работы
 */

#include "../include/service_controller.hpp"
#include <signal.h>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include "../include/FilterListManager.hpp"
#include "stc/compositelogger.hpp"
#include "stc/consolelogger.hpp"
#include "stc/asyncfilelogger.hpp"
#include "stc/syncfilelogger.hpp"
// #include "../include/signal_mask_guard.hpp"

using namespace std::chrono_literals;

int ServiceController::run(int argc, char **argv) {
    // SignalMaskGuard guard({SIGINT, SIGTERM, SIGHUP});

    sigset_t block_set;
sigemptyset(&block_set);
sigaddset(&block_set, SIGHUP);
pthread_sigmask(SIG_BLOCK, &block_set, nullptr);
std::cout << "Global block of SIGHUP installed" << std::endl;

    try {
        sigset_t initial_mask;
        pthread_sigmask(SIG_SETMASK, nullptr, &initial_mask);
        if (sigismember(&initial_mask, SIGHUP)) {
            std::cout << "DIAGNOSTIC: SIGHUP is BLOCKED at start of run()!" << std::endl;
        } else {
            std::cout << "DIAGNOSTIC: SIGHUP is NOT blocked at start of run()" << std::endl;
        }
        // Парсинг аргументов
        ArgumentParser parser;
        ParsedArgs args = parser.parse(argc, argv);
        pidFileMgr_ = std::make_unique<PidFileManager>(
    args.daemon_mode ? "/var/run/xmlfilter.pid"
                     : (std::getenv("HOME")
                          ? std::string(std::getenv("HOME")) + "/.xmlfilter.pid"
                          : "./xmlfilter.pid")
);

        if (args.help_message) {
            printHelp();
            return EXIT_SUCCESS;
        }
        if (args.version_message) {
            printVersion();
            return EXIT_SUCCESS;
        }
        if (args.reload) {
    if (auto pidOpt = pidFileMgr_->read()) {
            kill(*pidOpt, SIGHUP);
            std::cout << "Reload signal sent (PID " << *pidOpt << ")\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Service is not running (PID file not found)\n";
            return EXIT_FAILURE;
        }
}

        // Демонизация или запись PID для foreground
        config_path_ = args.config_path;
        if (args.daemon_mode) {
    daemon_ = std::make_unique<stc::DaemonManager>(pidFileMgr_->path().c_str());
    daemon_->daemonize();
    pidFileMgr_->write();
    pthread_sigmask(SIG_SETMASK, nullptr, &initial_mask);
            if (sigismember(&initial_mask, SIGHUP)) {
                std::cout << "DIAGNOSTIC: SIGHUP is BLOCKED after daemonize()!" << std::endl;
            }
} else {
    pidFileMgr_->write();
}
        
        stc::CompositeLogger::instance().info("FORCED SIGHUP unblock before initialize");
        // Загрузка конфигурации и логгера
        ConfigManager::instance().initialize(config_path_);
        if (!args.overrides.empty())
            ConfigManager::instance().applyCliOverrides(args.overrides);
        initLogger(args);

        // Регистрация сигналов и запуск Master
        initialize(args);
        stc::SignalRouter::instance().start();
        stc::CompositeLogger::instance().info("SignalRouter started successfully");

        // Инициализация CSV-фильтра
        std::string globalCsv =
            ConfigManager::instance().getGlobalComparisonList(args.environment);
        FilterListManager::instance().initialize(globalCsv);

        // Запуск обработки сигналов и главный цикл
        mainLoop();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e) {
        stc::CompositeLogger::instance().critical(e.what());
        if (daemon_) daemon_->cleanup();
        return EXIT_FAILURE;
    }
}

void ServiceController::initialize(const ParsedArgs &args) {
  auto &router = stc::SignalRouter::instance();
    stc::CompositeLogger::instance().debug("Service controller: Registering signal handlers ...");
  
    // Graceful shutdown на SIGTERM и SIGINT
    router.registerHandler(SIGTERM, [this](int sig_num){
        stc::CompositeLogger::instance().info("SIGTERM received (signal " + std::to_string(sig_num) + "), shutting down");
        handleShutdown();
    });
    router.registerHandler(SIGINT, [this](int sig_num){
        stc::CompositeLogger::instance().info("SIGINT received (signal " + std::to_string(sig_num) + "), shutting down");
        handleShutdown();
    });

    stc::CompositeLogger::instance().info("Registering SIGHUP handler...");
    
    // Проверить, что SIGHUP не заблокирован
    sigset_t current_mask;
    pthread_sigmask(SIG_SETMASK, nullptr, &current_mask);
    if (sigismember(&current_mask, SIGHUP)) {
        stc::CompositeLogger::instance().warning("SIGHUP is blocked before registration!");
    } else {
        stc::CompositeLogger::instance().info("SIGHUP is not blocked - good");
    }
    // Reload конфигурации на SIGHUP
    router.registerHandler(SIGHUP, [this, &args](int sig_num){
        stc::CompositeLogger::instance().info("SIGHUP handler called with signal: " + std::to_string(sig_num));
    stc::CompositeLogger::instance().info("SIGHUP received, starting reconfiguration");
    try {
        // Транзакционная перезагрузка конфигурации
        ConfigReloadTransaction tx(ConfigManager::instance());
        tx.reload();
        
        // Используем встроенный reload вместо полного пересоздания
        reloadWorkers(args);
        
        stc::CompositeLogger::instance().info("SIGHUP: configuration reloaded and workers restarted");
    } catch (const std::exception& e) {
        stc::CompositeLogger::instance().critical(
            std::string("SIGHUP: reload failed: ") + e.what()
        );
        // НЕ завершаем процесс при ошибке reload
    }
        });
    sigset_t test_mask;
pthread_sigmask(SIG_SETMASK, nullptr, &test_mask);
if (sigismember(&test_mask, SIGHUP)) {
    stc::CompositeLogger::instance().info("Post-register: SIGHUP is BLOCKED");
} else {
    stc::CompositeLogger::instance().error("Post-register: SIGHUP is NOT blocked");
}
    stc::CompositeLogger::instance().info("All signal handlers registered successfully");

    // Создаем и стартуем Master
    master_ = std::make_unique<Master>(
        [&args]() { return ConfigManager::instance().getMergedConfig(args.environment); }
    );
    master_->start();
}

void ServiceController::initLogger(const ParsedArgs &args) {
  auto &composite_logger = stc::CompositeLogger::instance();

  // Лямбда для безопасного создания shared_ptr из синглтона
  auto getSingletonPtr = [](auto &singleton) {
    return std::shared_ptr<std::remove_reference_t<decltype(singleton)>>(
        &singleton, [](auto *) {});
  };

  if (!args.use_cli_logging) {
    auto config = ConfigManager::instance().getMergedConfig(args.environment);
    if (config.contains("logging") && config["logging"].is_array()) {
      for (auto &entry : config["logging"]) {
        std::string type = entry.value("type", "console");
        std::string level = entry.value("level", "info");
        std::string file = entry.value("file", "service.log");
        // bool rotatad = entry.value("rotated", false);

        if (type == "console") {
          auto &logger = stc::ConsoleLogger::instance();
          logger.setLogLevel(stc::stringToLogLevel(level));
          composite_logger.addLogger(getSingletonPtr(logger));
        } else if (type == "async_file") {
          auto &logger = stc::AsyncFileLogger::instance();
          logger.setMainLogPath(file);
          logger.setLogLevel(stc::stringToLogLevel(level));
          composite_logger.addLogger(getSingletonPtr(logger));
        } else if (type == "sync_file") {
          auto &logger = stc::SyncFileLogger::instance();
          logger.setMainLogPath(file);
          logger.setLogLevel(stc::stringToLogLevel(level));
          composite_logger.addLogger(getSingletonPtr(logger));
        }
      }
    }
  } else if (!args.logger_types.empty()) {
    for (const auto &type : args.logger_types) {
      if (type == "console") {
        composite_logger.addLogger(
            getSingletonPtr(stc::ConsoleLogger::instance()));
      } else if (type == "async_file") {
        auto &logger = stc::AsyncFileLogger::instance();
        logger.setMainLogPath("async_service.log");
        composite_logger.addLogger(getSingletonPtr(logger));
      } else if (type == "sync_file") {
        auto &logger = stc::SyncFileLogger::instance();
        logger.setMainLogPath("sync_service.log");
        composite_logger.addLogger(getSingletonPtr(logger));
      }
    }
  } else {
    composite_logger.addLogger(getSingletonPtr(stc::ConsoleLogger::instance()));
  }

  if (args.log_level.has_value()) {
    composite_logger.setLogLevel(stc::stringToLogLevel(args.log_level.value()));
  }
}

void ServiceController::mainLoop() {
  std::unique_lock<std::mutex> lock(mtx_);
    running_ = true;
    
    stc::CompositeLogger::instance().info("Service controller: Service main loop started");
    
    while (!shutdown_requested_.load(std::memory_order_acquire)) {
        // Разблокируем мьютекс на время healthCheck
        lock.unlock();
        // sigset_t current_mask;
        // // pthread_sigmask(SIG_UNBLOCK, nullptr, &current_mask);
        // if (sigismember(&current_mask, SIGINT)) {
        //    stc::CompositeLogger::instance().warning("SIGINT is still blocked after reload!");
        // }
        stc::CompositeLogger::instance().debug("ServiceController::mainLoop() — shutdown_requested=" + std::to_string(shutdown_requested_.load()));
        master_->healthCheck();
        lock.lock();
        
        // Ждем сигнал завершения или таймаут
        cv_.wait_for(lock, std::chrono::milliseconds(500),
                     [this]{ return shutdown_requested_.load(std::memory_order_acquire); });
    }
    
    running_ = false;
    stc::CompositeLogger::instance().info("Service controller: Service main loop ended");
}

void ServiceController::handleShutdown() {
    stc::CompositeLogger::instance().debug("ServiceController::handleShutdown() ENTER");
    
    // Устанавливаем флаг завершения
    shutdown_requested_.store(true, std::memory_order_release);
    
    // Пробуждаем mainLoop
    {
        std::lock_guard lg(mtx_);
        running_ = false;
    }
    cv_.notify_one();
    
    // Останавливаем компоненты
    if (master_) {
        master_->stop();
    }
    
    // ВАЖНО: Удаляем PID файл только при полном shutdown
    if (pidFileMgr_) {
        pidFileMgr_->remove();  // Явно удаляем PID-файл
    }
    
    if (daemon_) {
        daemon_->cleanup();
    }
    
    stc::SignalRouter::instance().stop();
    stc::CompositeLogger::instance().info("Service controller: Service shutdown complete");
}

void ServiceController::reloadWorkers(const ParsedArgs &args) {
    stc::CompositeLogger::instance().info("ServiceController: Starting worker reload");
    (void)args;
    if (master_) {
        try {
            master_->reload();
            stc::CompositeLogger::instance().info("ServiceController: Workers reloaded successfully");
        } catch (const std::exception& e) {
            stc::CompositeLogger::instance().error(
                "ServiceController: Worker reload failed: " + std::string(e.what())
            );
        }
    } else {
        stc::CompositeLogger::instance().warning("ServiceController: No master to reload");
    }
}

void ServiceController::printHelp() {
  std::cout << "XML Filter Service\n\n"
       << "Usage:\n"
       << " service [options]\n\n"
       << "Options:\n"
       << " --help, -h          Show this help message\n"
       << " --version, -v       Show version info\n"
       << " --config-file=FILE  Configuration file path\n"
       << " --override=KEY:VAL  Override config parameter\n"
       << " --log-type=TYPES    Logger types (comma-separated)\n"
       << " --log-level=LEVEL   Logging level "
          "[debug|info|warning|error|critical]\n"
       << " --daemon            Run as daemon\n";
};

void ServiceController::printVersion() {
  std::cout << "XML Filter service v0.95.0\n"
            << "(c) 2025 by Artem Ulyanov, STC LLC.\n";
}