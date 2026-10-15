/**
 * @file service_controller.cpp
 * @brief Реализация методов ServiceController.
 * @version 4.1.0
 * @date 2026-07-24
 */
#include "service_controller.hpp"

#include <pthread.h>
#include <signal.h>
#include <fstream> 

#include <iostream>

#include "../infrastructure/logging/logger_factory.hpp"
#include "stc/metrics/metrics_registry.hpp"
#include "stc/metrics/prometheus_exporter.hpp"

using namespace std::chrono_literals;

namespace stc {

/** 
 * @brief Путь к PID-файлу по умолчанию.
 * @todo Сделать путь к PID-файлу настраиваемым через конфигурацию
 *        или использовать /tmp/ для локального тестирования.
 */
constexpr const char kPidFile[] = "/var/run/xmlfilter.pid";

constexpr const char kDefaultDbConfigPath[] = "/opt/xmlfilter/db_config.json";

constexpr const char kDefaultSqlitePath[] = "/opt/xmlfilter/data/app.db";

std::pair<std::string, int> ServiceController::LoadComparisonDbConfig() {
    std::string config_path = kDefaultDbConfigPath;
    if (const char* env_path = std::getenv("DB_CONFIG_PATH")) {
        config_path = env_path;
    }

    std::string db_path = kDefaultSqlitePath;
    int list_id = 1;

    std::ifstream file(config_path);
    if (file.is_open()) {
        try {
            nlohmann::json config;
            file >> config;
            if (config.contains("sqlite_db_path") && config["sqlite_db_path"].is_string()) {
                db_path = config["sqlite_db_path"].get<std::string>();
            }
            if (config.contains("comparison_list_id") && config["comparison_list_id"].is_number_integer()) {
                list_id = config["comparison_list_id"].get<int>();
            }
            if (logger_) {
                logger_->Info("Loaded comparison DB config from: " + config_path + 
                              " (db: " + db_path + ", list_id: " + std::to_string(list_id) + ")");
            }
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Warning("Failed to parse db_config.json: " + std::string(e.what()) + 
                                 ". Falling back to defaults.");
            }
        }
    } else {
        if (logger_) {
            logger_->Warning("db_config.json not found at " + config_path + ". Falling back to defaults.");
        }
    }
    return {db_path, list_id};
}

int ServiceController::Run(int argc, char** argv) {
  try {
    sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        sigaddset(&mask, SIGHUP);
        sigaddset(&mask, SIGUSR1); // Блокируем SIGUSR1 до инициализации SignalRouter
        if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0) {
            std::cerr << "Failed to block signals\n";
            return EXIT_FAILURE;
        }

        CliArguments cli_args = ArgumentParser().Parse(argc, argv);
        pid_file_mgr_ = std::make_unique<PidFileManager>(kPidFile);

    if (cli_args.help_requested) {
      PrintHelp();
      return EXIT_SUCCESS;
    }
    if (cli_args.version_requested) {
      PrintVersion();
      return EXIT_SUCCESS;
    }
    if (cli_args.reload_requested) {
      if (auto pid_opt = pid_file_mgr_->read()) {
        kill(*pid_opt, SIGHUP);
        std::cout << "Reload signal sent (PID " << *pid_opt << ")\n";
        return EXIT_SUCCESS;
      }
      std::cerr << "Service is not running\n";
      return EXIT_FAILURE;
    }

    pid_file_mgr_->write();

        // 1. Сборка конфигурации
        config_service_ = std::make_unique<ConfigurationService>();
        ApplicationConfiguration app_config = config_service_->BuildConfiguration(cli_args);

        // 2. Инициализация логгера
        logger_ = LoggerFactory::Create(app_config.loggers);
        logger_->Info("Configuration loaded successfully.");

        // 3. Инициализация метрик
        metrics_registry_ = std::make_shared<stc::metrics::MetricsRegistry>();
        RegisterGlobalMetrics();

        // 4. Инициализация FilterListManager (ПЕРЕХОД НА SQLITE)
        auto [db_path, list_id] = LoadComparisonDbConfig();
        sqlite_db_path_ = db_path;
        comparison_list_id_ = list_id;

        filter_list_manager_ = std::make_shared<FilterListManager>(logger_);
        filter_list_manager_->initialize(sqlite_db_path_, comparison_list_id_);

        if (global_metrics_.templates_count) {
            global_metrics_.templates_count->Set(
                static_cast<double>(filter_list_manager_->getTotalRecordsCount()));
        }

        // 5. Запуск оркестрации
        Initialize(app_config);
        MainLoop();
        HandleShutdown();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Critical(std::string("Fatal error: ") + e.what());
            logger_->Flush();
        } else {
            std::cerr << "Fatal error: " << e.what() << std::endl;
        }
        return EXIT_FAILURE;
    }
}

void ServiceController::RegisterGlobalMetrics() {
  global_metrics_.files_processed = metrics_registry_->RegisterCounter(
      "xml_files_processed_total", "Total processed");
  global_metrics_.files_failed = metrics_registry_->RegisterCounter(
      "xml_files_failed_total", "Total failed");
  global_metrics_.records_processed = metrics_registry_->RegisterCounter(
      "xml_records_processed_total", "Total records");
  global_metrics_.records_matched = metrics_registry_->RegisterCounter(
      "xml_records_matched_total", "Matched records");
  global_metrics_.bytes_processed = metrics_registry_->RegisterCounter(
      "xml_bytes_processed_total", "Total bytes");
  global_metrics_.templates_count = metrics_registry_->RegisterGauge(
      "xml_comparison_templates_count", "Templates count");
  global_metrics_.active_workers = metrics_registry_->RegisterGauge(
      "active_workers_count", "Active workers");
  global_metrics_.duration_avg = metrics_registry_->RegisterGauge(
      "xml_processing_duration_avg_seconds", "Avg duration");
  global_metrics_.duration_hist = metrics_registry_->RegisterHistogram(
      "xml_processing_duration_seconds", "Duration",
      {0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0});
}

void ServiceController::Initialize(const ApplicationConfiguration& config) {
    signal_router_ = std::make_unique<stc::signals::SignalRouter>();
    
    signal_router_->RegisterHandler(SIGTERM, [this](int) {
        shutdown_requested_.store(true, std::memory_order_release);
        cv_.notify_one();
    });
    signal_router_->RegisterHandler(SIGINT, [this](int) {
        shutdown_requested_.store(true, std::memory_order_release);
        cv_.notify_one();
    });
    signal_router_->RegisterHandler(SIGHUP, [this](int) { HandleSighup(); });
    
    // НОВОЕ: Регистрация SIGUSR1 для Hot-Reload списков сравнения
    signal_router_->RegisterHandler(SIGUSR1, [this](int) { HandleSigusr1(); });
    
    signal_router_->Start();
    logger_->Info("SignalRouter started successfully (SIGTERM, SIGINT, SIGHUP, SIGUSR1).");

    if (config.metrics_port > 0) {
        metrics_server_ = std::make_unique<MetricsHttpServer>(
            config.metrics_port,
            [this]() { return GetMetricsPayload(); }
        );
        metrics_server_->Start();
        if (logger_) logger_->Info("Metrics HTTP server started on port " + std::to_string(config.metrics_port));
    }

    master_ = std::make_unique<Master>(logger_, metrics_registry_, global_metrics_, filter_list_manager_);
    master_->start(config);
}

void ServiceController::HandleSighup() {
    if (logger_) logger_->Info("Received SIGHUP, initiating full configuration reload...");
    try {
        ConfigReloadTransaction tx(*config_service_, logger_);
        ApplicationConfiguration new_config = tx.reload();
        
        // ВНИМАНИЕ: filter_list_manager_->reload() БОЛЬШЕ НЕ ВЫЗЫВАЕТСЯ ЗДЕСЬ.
        // Списки сравнения теперь управляются исключительно через SIGUSR1 и SQLite.
        // Это позволяет избежать остановки и пересоздания пула воркеров при обновлении словарей.
        
        master_->reload(new_config);
        if (logger_) logger_->Info("SIGHUP: configuration and workers reloaded successfully.");
    } catch (const std::exception& e) {
        if (logger_) logger_->Critical("SIGHUP: reload failed: " + std::string(e.what()));
    }
}

void ServiceController::HandleSigusr1() {
    if (logger_) logger_->Info("Received SIGUSR1, initiating hot-reload of comparison lists from SQLite...");
    try {
        // Вызов hotReload использует паттерн Read-Copy-Update (RCU).
        // Воркеры не останавливаются, блокировка происходит только на микросекунды
        // в момент атомарной подмены указателя на хеш-таблицу.
        filter_list_manager_->hotReload();
        
        if (global_metrics_.templates_count) {
            global_metrics_.templates_count->Set(
                static_cast<double>(filter_list_manager_->getTotalRecordsCount()));
        }
        
        if (logger_) {
            logger_->Info("SIGUSR1 processing completed. Total records loaded: " + 
                          std::to_string(filter_list_manager_->getTotalRecordsCount()));
        }
    } catch (const std::exception& e) {
        // При ошибке чтения SQLite или парсинга JSON, старые данные в памяти сохраняются.
        if (logger_) {
            logger_->Error("SIGUSR1 hot-reload failed. Keeping old data in memory. Error: " + std::string(e.what()));
        }
    }
}

void ServiceController::MainLoop() {
  std::unique_lock<std::mutex> lock(mtx_);
  running_ = true;
  while (!shutdown_requested_.load(std::memory_order_acquire)) {
    lock.unlock();
    if (master_) master_->healthCheck();
    lock.lock();
    cv_.wait_for(lock, 500ms, [this] {
      return shutdown_requested_.load(std::memory_order_acquire);
    });
  }
  running_ = false;
}

void ServiceController::HandleShutdown() {
  if (logger_) logger_->Info("HandleShutdown: initiating graceful shutdown...");

  if (metrics_server_) {
        if (logger_) logger_->Info("HandleShutdown: stopping metrics HTTP server...");
        metrics_server_->Stop();
        metrics_server_.reset();
  }

  if (master_) {
    if (logger_)
      logger_->Info("HandleShutdown: stopping master and workers...");
    master_->stop();
    if (logger_) logger_->Info("HandleShutdown: master stopped.");
  }

  if (signal_router_) {
    if (logger_) logger_->Info("HandleShutdown: stopping signal router...");
    signal_router_->Stop();
    if (logger_) logger_->Info("HandleShutdown: signal router stopped.");
  }

  if (pid_file_mgr_) pid_file_mgr_->remove();
  
  if (logger_) {
    logger_->Info("Service shutdown complete.");
    logger_->Flush();
  }
}

std::string ServiceController::GetMetricsPayload() const {
  if (!metrics_registry_) return "";
  return stc::metrics::ExportToPrometheus(*metrics_registry_, "xmlfilter");
}

void ServiceController::PrintHelp() {
  std::cout << "Usage: xmlfilter [options]\n";
}

void ServiceController::PrintVersion() { std::cout << "XML Filter Service v4.0.0\n"; }

}  // namespace stc