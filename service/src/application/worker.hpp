/**
@file worker.hpp
@brief Рабочий поток обработки файлов из одного источника.
@version 3.2.0
@date 2026-07-24
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

#include "../domain/FilterListManager.hpp"
#include "../domain/DTO/metrics_descriptors.hpp"
#include "../domain/sourceconfig.hpp"
#include "stc/logger/ilogger.hpp"
#include "../domain/ports/Ifilestorage.hpp"

namespace fs = std::filesystem;

namespace stc {

/**
@class Worker
@brief Инкапсулирует цикл мониторинга и обработки файлов для одного источника.
*/
class Worker {
public:
    /**
    @brief Конструктор Worker с инъекцией зависимостей.
    @param[in] config Параметры источника данных.
    @param[in] logger Диспетчер логирования.
    @param[in] filter_list_manager Менеджер списков фильтрации.
    @param[in] global_metrics Дескрипторы общих метрик.
    @param[in] source_metrics Дескрипторы детализированных метрик источника.
    */
    Worker(const SourceConfig &config,
           std::shared_ptr<stc::logger::ILogger> logger,
           std::shared_ptr<FilterListManager> filter_list_manager,
           GlobalMetricsDescriptors global_metrics,
           SourceMetricsDescriptors source_metrics);

    /// @brief Деструктор. Останавливает поток и освобождает ресурсы.
    ~Worker();

    /// @brief Запускает поток и мониторинг файлового хранилища.
    void start();

    /// @brief Останавливает поток и мониторинг.
    void stop();

    /// @brief Приостанавливает обработку новых файлов.
    void pause();

    /// @brief Возобновляет обработку файлов.
    void resume();

    /// @brief Полностью перезапускает воркер.
    void restart();

    /// @brief Останавливает воркер, ожидая завершения текущей обработки файла.
    void stopGracefully();

    /// @brief Проверяет, активен ли поток воркера.
    /// @return true Если поток активен.
    bool isAlive() const noexcept;

    /** @brief Проверяет, находится ли воркер на паузе.
     * @return true Если воркер на паузе.*/
    bool isPaused() const noexcept;

    /** @brief Возвращает конфигурацию источника.
     * @return Константная ссылка на SourceConfig.*/
    const SourceConfig &getConfig() const noexcept;

    /// @brief Перезапускает только подсистему мониторинга адаптера.
    void restartMonitoring();

private:
    /**
     * @private 
     * @brief Основной цикл потока воркера.*/
    void run();

    /** @private 
     * @brief Обрабатывает обнаруженный файл.
     * @param[in] filePath Абсолютный путь к файлу.*/
    void processFile(const std::string &filePath);

    /** @private 
     * @brief Проверяет и создает необходимые директории.*/
    void validatePaths() const;

    /** @private 
     * @brief Вычисляет SHA-256 хэш файла.
     * @param[in] filePath Путь к файлу.
     * @return std::string Hex-представление хэша.
     */
    std::string getFileHash(const std::string &filePath) const;

    /** @private Формирует путь для перемещения обработанного файла.
     * @param[in] originalPath Исходный путь.
     * @return std::string Целевой путь.*/
    std::string getFilteredFilePath(const std::string &originalPath) const;

    /** @private 
     * @brief Перемещает файл в директорию обработанных.
     * @param[in] filePath Исходный путь.
     * @param[in] processedPath Целевой путь.*/
    void moveToProcessed(const std::string &filePath, const std::string &processedPath);

    /** @private 
     * @brief Перемещает файл в директорию ошибок.
     * @param[in] filePath Исходный путь.
     * @param[in] error Текст ошибки.*/
    void handleFileError(const std::string &filePath, const std::string &error);

    /** @private 
     * @brief Обновляет скользящее среднее время обработки.
     * @param[in] duration Длительность обработки в секундах.*/
    void updateAverageDuration(double duration);

    /** @private 
     * @brief Конфигурация источника.*/
    SourceConfig config_;

    /** @private 
     * @brief Уникальный тег воркера для логирования.*/
    std::string workerTag_;

    /** @private 
     * @brief Глобальный счетчик экземпляров воркеров.*/
    static std::atomic<int> instanceCounter_;

    /** @private 
     * @brief Полиморфный адаптер файлового хранилища.*/
    std::unique_ptr<IFileStorage> adapter_;

    /** @private 
     * @brief Диспетчер логирования.*/
    std::shared_ptr<stc::logger::ILogger> logger_;

    /** @private 
     * @brief Менеджер списков фильтрации.*/
    std::shared_ptr<FilterListManager> filter_list_manager_;

    /** @private 
     * @brief Дескрипторы общих метрик сервиса.*/
    GlobalMetricsDescriptors global_metrics_;

    /** @private 
     * @brief Дескрипторы детализированных метрик источника.*/
    SourceMetricsDescriptors source_metrics_;

    /** @private 
     * @brief Флаг активности потока.*/
    std::atomic<bool> running_{false};

    /** @private 
     * @brief Флаг паузы.*/
    std::atomic<bool> paused_{false};

    /** @private 
     * @brief Флаг текущей обработки файла.*/
    std::atomic<bool> processing_{false};

    /** @private 
     * @brief Поток выполнения.*/
    std::thread worker_thread_;

    /** @private 
     * @brief Мьютекс для синхронизации состояния.*/
    mutable std::mutex state_mutex_;

    /** @private 
     * @brief Условная переменная для паузы/остановки.*/
    std::condition_variable cv_;

    /** @private 
     * @brief Время старта воркера.*/
    std::chrono::steady_clock::time_point start_time_;

    /** @private 
     * @brief Счётчик обработанных файлов.*/
    std::atomic<uint64_t> file_count_{0};

    /** @private 
     * @brief Текущее скользящее среднее время обработки.*/
    std::atomic<double> avg_duration_{0.0};
};

} // namespace stc