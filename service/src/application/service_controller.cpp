/**
@file service_controller.cpp
@brief Реализация методов ServiceController.
@version 3.1.1
@date 2026-07-24
*/
#include "service_controller.hpp"
#include <signal.h>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include "../domain/FilterListManager.hpp"
#include "../infrastructure/logging/logger_factory.hpp"
#include "stc/metrics/metrics_registry.hpp"
#include "stc/metrics/noop_metrics.hpp"
#include "stc/metrics/prometheus_exporter.hpp"
#include "stc/signals/signal_router.hpp"
#include "stc/signals/system_calls.hpp"

using namespace std::chrono_literals;

namespace stc {

constexpr const char* kPidFile = "/var/run/xmlfilter.pid";

int ServiceController::Run(int argc, char **argv) {
    try {
        ArgumentParser parser;
        ParsedArgs args = parser.Parse(argc, argv);

        pid_file_mgr_ = std::make_unique<PidFileManager>(kPidFile);

        if (args.help_message_) {
            PrintHelp();
            return EXIT_SUCCESS;
        }
        if (args.version_message_) {
            PrintVersion();
            return EXIT_SUCCESS;
        }
        if (args.reload_) {
            if (auto pid_opt = pid_file_mgr_->read()) {
                kill(*pid_opt, SIGHUP);
                std::cout << "Reload signal sent (PID " << *pid_opt << ")\n";
                return EXIT_SUCCESS;
            } else {
                std::cerr << "Service is not running (PID file not found)\n";
                return EXIT_FAILURE;
            }
        }

        config_path_ = args.config_path_;
        pid_file_mgr_->write();
        ConfigManager::instance().initialize(config_path_);

        if (!args.overrides_.empty()) {
            ConfigManager::instance().applyCliOverrides(args.overrides_);
        }

        InitLogger(args);
        logger_->Info("Configuration loaded successfully.");

        metrics_registry_ = std::make_shared<stc::metrics::MetricsRegistry>();
        RegisterGlobalMetrics();

        // ИНИЦИАЛИЗАЦИЯ FilterListManager через DI (Единственный корректный блок)
        std::string global_csv = ConfigManager::instance().getGlobalComparisonList(args.environment_);
        filter_list_manager_ = std::make_shared<FilterListManager>(logger_);
        filter_list_manager_->initialize(global_csv);

        if (global_metrics_.templates_count) {
            global_metrics_.templates_count->Set(
                static_cast<double>(filter_list_manager_->getTotalRecordsCount()));
            logger_->Info("Global templates count metric updated: " + 
                          std::to_string(filter_list_manager_->getTotalRecordsCount()));
        }

        Initialize(args);
        signal_router_->Start();
        logger_->Info("SignalRouter started successfully.");
        
        MainLoop();
        HandleShutdown();
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        if (logger_) {
            logger_->Critical(std::string("Fatal error: ") + e.what());
            logger_->Flush();
        } else {
            std::cerr << "Fatal error (logger not initialized): " << e.what() << std::endl;
        }
        return EXIT_FAILURE;
    }
}

void ServiceController::RegisterGlobalMetrics() {
    // Counters
    global_metrics_.files_processed = metrics_registry_->RegisterCounter("xml_files_processed_total", "Total successfully processed files");
    global_metrics_.files_failed = metrics_registry_->RegisterCounter("xml_files_failed_total", "Total failed files");
    global_metrics_.records_processed = metrics_registry_->RegisterCounter("xml_records_processed_total", "Total processed XML records");
    global_metrics_.records_matched = metrics_registry_->RegisterCounter("xml_records_matched_total", "Total matched XML records");
    global_metrics_.bytes_processed = metrics_registry_->RegisterCounter("xml_bytes_processed_total", "Total processed bytes");

    // Gauges
    global_metrics_.templates_count = metrics_registry_->RegisterGauge("xml_comparison_templates_count", "Current templates count");
    global_metrics_.active_workers = metrics_registry_->RegisterGauge("active_workers_count", "Current active workers");
    global_metrics_.duration_avg = metrics_registry_->RegisterGauge("xml_processing_duration_avg_seconds", "Sliding average processing time");

    // Histogram
    global_metrics_.duration_hist = metrics_registry_->RegisterHistogram(
        "xml_processing_duration_seconds", 
        "Processing duration", 
        {0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0});

    logger_->Info("Global metrics registered successfully.");
}

void ServiceController::Initialize(const ParsedArgs &args) {
    signal_router_ = std::make_unique<stc::signals::SignalRouter>();

    logger_->Debug("Service controller: Registering signal handlers...");

    signal_router_->RegisterHandler(SIGTERM, [this](int) {
        if (logger_) logger_->Info("SIGTERM received, initiating shutdown...");
        shutdown_requested_.store(true, std::memory_order_release);
        cv_.notify_one();
    });

    signal_router_->RegisterHandler(SIGINT, [this](int) {
        if (logger_) logger_->Info("SIGINT received, initiating shutdown...");
        shutdown_requested_.store(true, std::memory_order_release);
        cv_.notify_one();
    });

    signal_router_->RegisterHandler(SIGHUP, [this, args](int) {
        if (logger_) logger_->Info("SIGHUP received, starting reconfiguration.");
        try {
            ConfigReloadTransaction tx(ConfigManager::instance(), logger_);
            tx.reload();

            filter_list_manager_->reload();
            
            if (global_metrics_.templates_count) {
                global_metrics_.templates_count->Set(
                    static_cast<double>(filter_list_manager_->getTotalRecordsCount()));
            }

            ReloadWorkers(args);
            if (logger_) logger_->Info("SIGHUP: configuration reloaded and workers restarted.");
        } catch (const std::exception& e) {
            if (logger_) logger_->Critical(std::string("SIGHUP: reload failed: ") + e.what());
        }
    });

    // Инъекция зависимостей в Master
    master_ = std::make_unique<Master>(
        [&args]() { return ConfigManager::instance().getMergedConfig(args.environment_); },
        logger_,
        metrics_registry_,
        global_metrics_,
        filter_list_manager_
    );
    master_->start();
}

void ServiceController::InitLogger(const ParsedArgs &args) {
    std::vector<LoggerSinkConfig> sinks_config;
    if (args.use_cli_logging_) {
        // Логика формирования sinks_config из CLI (упрощенно)
    } else {
        sinks_config = ConfigManager::instance().getLoggingSinksConfig(args.environment_);
    }
    logger_ = LoggerFactory::Create(sinks_config);
    logger_->Info("Logger subsystem initialized.");
}

void ServiceController::MainLoop() {
    // Исправлено: угловые скобки вместо квадратных
    std::unique_lock<std::mutex> lock(mtx_);
    running_ = true;
    logger_->Info("Service controller: Service main loop started.");

    while (!shutdown_requested_.load(std::memory_order_acquire)) {
        lock.unlock();
        master_->healthCheck();
        lock.lock();
        cv_.wait_for(lock, 500ms, [this] { 
            return shutdown_requested_.load(std::memory_order_acquire); 
        });
    }
    running_ = false;
    logger_->Info("Service controller: Service main loop ended.");
}

void ServiceController::HandleShutdown() {
    logger_->Debug("ServiceController::HandleShutdown() ENTER");
    
    if (master_) {
        master_->stop();
    }
    if (signal_router_) {
        signal_router_->Stop(); // Теперь это безопасно, так как мы не в потоке сигналов
    }
    if (pid_file_mgr_) {
        pid_file_mgr_->remove();
    }
    if (logger_) {
        logger_->Info("Service controller: Service shutdown complete.");
        logger_->Flush();
    }
}

// Исправлено: сигнатура метода приведена в соответствие с .hpp, 
// некорректный комментарий / args / заменен на валидный /* args */
void ServiceController::ReloadWorkers(const ParsedArgs& /*args*/) {
    logger_->Info("ServiceController: Starting worker reload");
    if (master_) {
        try {
            master_->reload();
            logger_->Info("ServiceController: Workers reloaded successfully");
        } catch (const std::exception & e) {
            logger_->Error("ServiceController: Worker reload failed: " + std::string(e.what()));
        }
    } else {
        logger_->Warning("ServiceController: No master to reload");
    }
}

std::string ServiceController::GetMetricsPayload() const {
    if (!metrics_registry_) {
        return "";
    }
    // Формируем снимок состояния с префиксом неймспейса "xmlfilter"
    return stc::metrics::ExportToPrometheus(*metrics_registry_, "xmlfilter");
}

void ServiceController::PrintHelp() {
    std::cout << "XML Filter Service\n\n"
              << "Usage:\n"
              << "  service [options]\n\n"
              << "Options:\n"
              << "  --help, -h          Show this help message\n"
              << "  --version, -v       Show version info\n"
              << "  --config-file=FILE  Configuration file path\n"
              << "  --override=KEY:VAL  Override config parameter\n"
              << "  --log-type=TYPES    Logger types (comma-separated)\n"
              << "  --log-level=LEVEL   Logging level\n"
              << "  --reload, -r        Send SIGHUP to running instance\n";
}

void ServiceController::PrintVersion() {
    std::cout << "XML Filter service v1.0.0\n"
              << "(c) 2026 by Artem Ulyanov, STC LLC.\n";
}

} // namespace stc