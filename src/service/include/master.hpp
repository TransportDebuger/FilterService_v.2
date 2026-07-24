/**
@file master.hpp
@brief Оркестратор рабочих потоков и менеджер жизненного цикла источников.
@version 3.0.0
@date 2026-07-21
*/
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "../include/AdapterFactory.hpp"
#include "../include/workercontainer.hpp"
#include "../include/DTO/metrics_descriptors.hpp"
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
    @param[in] logger Умный указатель на диспетчер логирования.
    @param[in] registry Реестр метрик для регистрации детализированных показателей.
    @param[in] global_metrics Дескрипторы общих (агрегированных) метрик сервиса.
    */
    Master(std::function<nlohmann::json()> configProvider,
           std::shared_ptr<stc::logger::ILogger> logger,
           std::shared_ptr<stc::metrics::IMetricsRegistry> registry,
           GlobalMetricsDescriptors global_metrics);

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

    /**
    @private Получает или создает дескрипторы детализированных метрик для источника.
    @param[in] source_name Уникальное имя источника (из поля name в config.json).
    @return SourceMetricsDescriptors Набор дескрипторов для инъекции в Worker.
    */
    SourceMetricsDescriptors getOrCreateSourceMetrics(const std::string& source_name);

    /// @private Контейнер активных воркеров.
    WorkersContainer workers_;
    
    /// @private Провайдер конфигурации.
    std::function<nlohmann::json()> getConfig_;
    
    /// @private Маршрутизатор сигналов (для внутренней синхронизации, если требуется).
    std::shared_ptr<stc::SignalRouter> signalRouter_;
    
    /// @private Диспетчер логирования, полученный через DI.
    std::shared_ptr<stc::logger::ILogger> logger_;
    
    /// @private Реестр метрик для регистрации новых показателей при реконфигурации.
    std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
    
    /// @private Дескрипторы общих метрик сервиса.
    GlobalMetricsDescriptors global_metrics_;
    
    /// @private Кэш дескрипторов детализированных метрик (ключ - имя источника).
    /// Гарантирует монотонность счетчиков при SIGHUP (переиспользование атомарных ячеек).
    std::unordered_map<std::string, SourceMetricsDescriptors> source_metrics_cache_;

    /// @private Текущее состояние мастера.
    std::atomic<State> state_{State::STOPPED};
    
    /// @private Мьютекс для защиты операций с конфигурацией.
    mutable std::mutex configMutex_;
};

} // namespace stc