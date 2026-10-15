/**
@file service_controller.hpp
@brief Корень композиции и контроллер жизненного цикла сервиса.
@version 4.2.0
@date 2026-10-24
*/
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <utility> // Для std::pair

#include "../domain/DTO/metrics_descriptors.hpp"
#include "../domain/FilterListManager.hpp"
#include "../domain/application_configuration.hpp"
#include "../infrastructure/configuration/config_reload_transaction.hpp"
#include "../infrastructure/configuration/configuration_service.hpp"
#include "../infrastructure/system/pid_file_manager.hpp"
#include "../infrastructure/metrics/metrics_http_server.hpp"
#include "argumentparser.hpp"
#include "master.hpp"
#include "stc/logger/ilogger.hpp"
#include "stc/metrics/imetrics_registry.hpp"
#include "stc/signals/signal_router.hpp"
#include "stc/signals/system_calls.hpp"

namespace stc {

class ServiceController {
public:
    int Run(int argc, char** argv); 
    [[nodiscard]] std::string GetMetricsPayload() const;

private:
    void Initialize(const ApplicationConfiguration& config);
    void RegisterGlobalMetrics();
    void MainLoop();
    void HandleShutdown();
    
    /// @brief Обработчик SIGHUP (транзакционная перезагрузка конфигурации и воркеров).
    void HandleSighup();
    
    /// @brief Обработчик SIGUSR1 (Hot-Reload списков сравнения из SQLite).
    void HandleSigusr1();

    /// @brief Загружает путь к SQLite и ID списка из db_config.json или ENV.
    /// @return std::pair<std::string, int> Путь к БД и ID списка.
    std::pair<std::string, int> LoadComparisonDbConfig();

    void PrintHelp();
    void PrintVersion();

    std::unique_ptr<PidFileManager> pid_file_mgr_;
    std::shared_ptr<stc::logger::ILogger> logger_;
    std::unique_ptr<Master> master_;
    std::unique_ptr<stc::signals::SignalRouter> signal_router_;
    std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
    std::shared_ptr<stc::FilterListManager> filter_list_manager_;
    std::unique_ptr<ConfigurationService> config_service_;
    std::unique_ptr<MetricsHttpServer> metrics_server_;
    
    GlobalMetricsDescriptors global_metrics_;
    
    std::atomic<bool> running_{false};
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_requested_{false};

    /// @private Путь к SQLite БД (кэшируется при старте для использования в SIGUSR1).
    std::string sqlite_db_path_;
    /// @private ID списка сравнения (кэшируется при старте для использования в SIGUSR1).
    int comparison_list_id_{1};
};

}  // namespace stc