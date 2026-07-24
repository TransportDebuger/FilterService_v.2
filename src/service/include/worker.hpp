/**
@file worker.hpp
@brief Рабочий поток обработки файлов из одного источника.
@version 3.1.0
@date 2026-07-21
*/
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "../include/AdapterFactory.hpp"
#include "../include/filestorageinterface.hpp"
#include "../include/sourceconfig.hpp"
#include "../include/DTO/metrics_descriptors.hpp"
#include "stc/logger/ilogger.hpp"

namespace fs = std::filesystem;

namespace stc {

/**
@class Worker
@brief Инкапсулирует цикл мониторинга и обработки файлов для одного источника.
*/
class Worker {
public:
    /**
    @brief Конструктор Worker с инъекцией логгера и дескрипторов метрик.
    @param[in] config Параметры источника данных.
    @param[in] logger Умный указатель на диспетчер логирования.
    @param[in] global_metrics Дескрипторы общих (агрегированных) метрик.
    @param[in] source_metrics Дескрипторы детализированных (пер-источниковых) метрик.
    */
    Worker(const SourceConfig &config,
           std::shared_ptr<stc::logger::ILogger> logger,
           GlobalMetricsDescriptors global_metrics,
           SourceMetricsDescriptors source_metrics);

    ~Worker();

    void start();
    void stop();
    void pause();
    void resume();
    void restart();
    void stopGracefully();

    bool isAlive() const noexcept;
    bool isPaused() const noexcept;
    const SourceConfig &getConfig() const noexcept { return config_; }
    void restartMonitoring();

private:
    void run();
    void processFile(const std::string &filePath);
    void validatePaths() const;
    std::string getFileHash(const std::string &filePath) const;
    std::string getFilteredFilePath(const std::string &originalPath) const;
    void moveToProcessed(const std::string &filePath, const std::string &processedPath);
    void handleFileError(const std::string &filePath, const std::string &error);
    
    /**
    @private Обновляет скользящее среднее время обработки и соответствующие Gauge-метрики.
    @param[in] duration Длительность обработки текущего файла (секунды).
    */
    void updateAverageDuration(double duration);

    /// @private Конфигурация источника.
    SourceConfig config_;
    
    /// @private Уникальный тег воркера для логирования.
    std::string workerTag_;
    
    /// @private Глобальный счетчик экземпляров воркеров.
    static std::atomic<int> instanceCounter_;

    /// @private Полиморфный адаптер файлового хранилища (Local, SMB, FTP).
    std::unique_ptr<FileStorageInterface> adapter_;
    
    /// @private Диспетчер логирования, полученный через DI.
    std::shared_ptr<stc::logger::ILogger> logger_;
    
    /// @private Дескрипторы общих метрик сервиса.
    GlobalMetricsDescriptors global_metrics_;
    
    /// @private Дескрипторы детализированных метрик источника.
    SourceMetricsDescriptors source_metrics_;

    /// @private Флаг активности потока.
    std::atomic<bool> running_{false};
    
    /// @private Флаг паузы.
    std::atomic<bool> paused_{false};
    
    /// @private Флаг текущей обработки файла (для graceful shutdown).
    std::atomic<bool> processing_{false};
    
    /// @private Поток выполнения.
    std::thread worker_thread_;
    
    /// @private Мьютекс для синхронизации состояния.
    mutable std::mutex state_mutex_;
    
    /// @private Условная переменная для паузы/остановки.
    std::condition_variable cv_;
    
    /// @private Время старта воркера.
    std::chrono::steady_clock::time_point start_time_;

    /// @private Счётчик обработанных файлов для вычисления скользящего среднего.
    std::atomic<uint64_t> file_count_{0};
    
    /// @private Текущее скользящее среднее время обработки.
    std::atomic<double> avg_duration_{0.0};
};

} // namespace stc