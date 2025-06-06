#pragma once
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <string>

/**
 * @class MetricsCollector
 * @brief Система сбора метрик производительности
 * 
 * @note Реализует паттерн Singleton для глобального доступа
 * Поддерживает основные типы метрик: счетчики, гистограммы, тайминги
 * Обеспечивает потокобезопасность через std::mutex
 */
class MetricsCollector {
public:
    static MetricsCollector& instance();
    
    /**
     * @brief Регистрирует новый счетчик
     * @param name Уникальное имя метрики
     */
    void registerCounter(const std::string& name);
    
    /**
     * @brief Увеличивает значение счетчика
     * @param name Имя метрики
     * @param value Значение для добавления (по умолчанию 1)
     */
    void incrementCounter(const std::string& name, double value = 1.0);
    
    /**
     * @brief Экспортирует метрики в формате Prometheus
     * @return std::string Строка с метриками в текстовом формате
     */
    std::string exportPrometheus() const;

private:
    MetricsCollector() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::atomic<double>> counters_;
};