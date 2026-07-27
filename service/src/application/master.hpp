/**
@file master.hpp
@brief Оркестратор рабочих потоков и менеджер жизненного цикла источников.
@version 3.2.0
@date 2026-07-24
*/
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "workercontainer.hpp"
#include "../domain/FilterListManager.hpp"
#include "../domain/DTO/metrics_descriptors.hpp"
#include "stc/metrics/imetrics_registry.hpp"
#include "stc/signals/signal_router.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class Master
@brief Управляет созданием, перезагрузкой и мониторингом пула воркеров.
*/
class Master {
public:
    enum class State { STOPPED, STARTING, RUNNING, RELOADING, FATAL };

    /**
    @brief Конструктор мастера с инъекцией зависимостей.
    @param[in] configProvider Функция для получения актуальной JSON-конфигурации.
    @param[in] logger Диспетчер логирования.
    @param[in] registry Реестр метрик.
    @param[in] global_metrics Дескрипторы общих метрик сервиса.
    @param[in] filter_list_manager Менеджер списков фильтрации.
    */
    Master(std::function<nlohmann::json()> configProvider,
           std::shared_ptr<stc::logger::ILogger> logger,
           std::shared_ptr<stc::metrics::IMetricsRegistry> registry,
           GlobalMetricsDescriptors global_metrics,
           std::shared_ptr<FilterListManager> filter_list_manager);

    ~Master();

    bool start();
    void stop() noexcept;
    void reload();
    void healthCheck();
    
    State getState() const noexcept;
    size_t getWorkerCount() const;
    void validateConfig(const nlohmann::json &config) const;
    void restartAllMonitoring();

private:
    void spawnWorkers();
    void terminateWorkers();
    SourceMetricsDescriptors getOrCreateSourceMetrics(const std::string& source_name);

    /// @private Контейнер активных воркеров.
    WorkersContainer workers_;
    /// @private Провайдер конфигурации.
    std::function<nlohmann::json()> getConfig_;
    /// @private Диспетчер логирования.
    std::shared_ptr<stc::logger::ILogger> logger_;
    /// @private Реестр метрик.
    std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
    /// @private Дескрипторы общих метрик сервиса.
    GlobalMetricsDescriptors global_metrics_;
    /// @private Менеджер списков фильтрации.
    std::shared_ptr<FilterListManager> filter_list_manager_;
    /// @private Кэш дескрипторов детализированных метрик.
    std::unordered_map<std::string, SourceMetricsDescriptors> source_metrics_cache_;
    /// @private Текущее состояние мастера.
    std::atomic<State> state_{State::STOPPED};
    /// @private Мьютекс для защиты операций с конфигурацией.
    mutable std::mutex configMutex_;
};

} // namespace stc