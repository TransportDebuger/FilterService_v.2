/**
@file service_controller.cpp
@brief Реализация методов ServiceController.
@version 4.0.0
@date 2026-07-24
*/
#include "service_controller.hpp"

#include <pthread.h>
#include <signal.h>

#include <iostream>

#include "../infrastructure/logging/logger_factory.hpp"
#include "stc/metrics/metrics_registry.hpp"
#include "stc/metrics/prometheus_exporter.hpp"

using namespace std::chrono_literals;

namespace stc {

constexpr const char* kPidFile = "/var/run/xmlfilter.pid";

int ServiceController::Run(int argc, char** argv) {
  try {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
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

    // 1. Сборка конфигурации (Configuration Pipeline)
    config_service_ = std::make_unique<ConfigurationService>();
    ApplicationConfiguration app_config =
        config_service_->BuildConfiguration(cli_args);

    // 2. Инициализация логгера из строгого DTO
    logger_ = LoggerFactory::Create(app_config.loggers);
    logger_->Info("Configuration loaded successfully.");

    // 3. Инициализация метрик
    metrics_registry_ = std::make_shared<stc::metrics::MetricsRegistry>();
    RegisterGlobalMetrics();

    // 4. Инициализация FilterListManager
    filter_list_manager_ = std::make_shared<FilterListManager>(logger_);
    filter_list_manager_->initialize(app_config.global_comparison_list);
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

  signal_router_->Start();
  logger_->Info("SignalRouter started successfully.");

  master_ = std::make_unique<Master>(logger_, metrics_registry_,
                                     global_metrics_, filter_list_manager_);
  master_->start(config);
}

void ServiceController::HandleSighup() {
  try {
    ConfigReloadTransaction tx(*config_service_, logger_);
    ApplicationConfiguration new_config = tx.reload();

    filter_list_manager_->reload();
    if (global_metrics_.templates_count) {
      global_metrics_.templates_count->Set(
          static_cast<double>(filter_list_manager_->getTotalRecordsCount()));
    }

    master_->reload(new_config);
    if (logger_) logger_->Info("SIGHUP: configuration reloaded successfully.");
  } catch (const std::exception& e) {
    if (logger_)
      logger_->Critical("SIGHUP: reload failed: " + std::string(e.what()));
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
void ServiceController::PrintVersion() { std::cout << "XML Filter v4.0.0\n"; }

}  // namespace stc