/**
@file service_controller.hpp
@brief Класс управления жизненным циклом сервиса.
@version 3.1.0
@date 2026-07-21
*/
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "../include/argumentparser.hpp"
#include "../include/config_reload_transaction.hpp"
#include "../include/configmanager.hpp"
#include "../include/master.hpp"
#include "../include/pid_file_manager.hpp"
#include "../include/DTO/metrics_descriptors.hpp"

#include "stc/metrics/imetrics_registry.hpp"
#include "stc/signals/signal_router.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class ServiceController
@brief Управляет запуском, конфигурацией и жизненным циклом сервиса.
*/
class ServiceController {
public:
    /**
    @brief Основная точка входа сервиса.
    @param[in] argc Количество аргументов командной строки.
    @param[in] argv Массив аргументов.
    @return int Код завершения (EXIT_SUCCESS или EXIT_FAILURE).
    */
    int Run(int argc, char **argv);

    /**
    @brief Формирует текстовый снимок метрик в формате Prometheus Exposition.
    @return std::string Строка с метриками для передачи по HTTP.
    */
    [[nodiscard]] std::string GetMetricsPayload() const;

private:
    /**
    @private Настраивает SignalRouter и Master.
    @param[in] args Результаты парсинга CLI.
    */
    void Initialize(const ParsedArgs &args);

    /**
    @private Инициализирует подсистему логирования.
    @param[in] args ParsedArgs, содержащий параметры логирования.
    */
    void InitLogger(const ParsedArgs &args);

    /**
    @private Регистрирует общие (агрегированные) метрики в реестре.
    */
    void RegisterGlobalMetrics();

    /**
    @private Главный цикл работы сервиса.
    */
    void MainLoop();

    /**
    @private Обработчик сигналов SIGTERM и SIGINT.
    */
    void HandleShutdown();

    /**
    @private Перезагружает конфигурацию и воркеров.
    @param[in] args Аргументы командной строки.
    */
    void ReloadWorkers(const ParsedArgs &args);

    /**
    @private Выводит справочную информацию.
    */
    void PrintHelp();

    /**
    @private Выводит информацию о версии.
    */
    void PrintVersion();

    /// @private Менеджер PID-файла.
    std::unique_ptr<PidFileManager> pid_file_mgr_;
    
    /// @private Диспетчер логирования.
    std::shared_ptr<stc::logger::ILogger> logger_;
    
    /// @private Экземпляр основного менеджера обработки задач.
    std::unique_ptr<Master> master_;
    
    /// @private Маршрутизатор сигналов. 
    std::unique_ptr<stc::signals::SignalRouter> signal_router_;
    
    /// @private Реестр метрик (создается один раз, живет весь жизненный цикл сервиса).
    std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
    
    /// @private Дескрипторы общих метрик (передаются в Master и далее в Worker).
    GlobalMetricsDescriptors global_metrics_;

    /// @private Путь к файлу конфигурации.
    std::string config_path_ = "/etc/xmlfilter/config.json";
    
    /// @private Флаг состояния основного цикла.
    std::atomic<bool> running_{false};
    
    /// @private Мьютекс для синхронизации.
    std::mutex mtx_;
    
    /// @private Условная переменная для прерывания ожидания.
    std::condition_variable cv_;
    
    /// @private Флаг запроса завершения работы.
    std::atomic<bool> shutdown_requested_{false};
};

} // namespace stc