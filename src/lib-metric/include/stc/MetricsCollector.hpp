/**
 * @file MetricsCollector.hpp
 * @brief Система сбора метрик в формате Prometheus
 * 
 * @author Artem Ulyanov
 * @date Май 2025
 * @version 1.0
 * @license MIT
 * 
 * @details Реализует потокобезопасный сбор:
 * - Счетчиков (Counter)
 * - Времени выполнения задач (Summary)
 * - Поддерживает экспорт в Prometheus-совместимом формате
 */

#pragma once
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <string>
#include <sstream>
#include <chrono>

namespace stc {

/**
 * @class MetricsCollector
 * @brief Потокобезопасный сборщик метрик с поддержкой Prometheus
 * 
 * @note Реализованные паттерны:
 * - Singleton (единственный экземпляр)
 * - Thread-safe (блокировки для конкурентного доступа)
 * 
 * @warning 
 * - Не поддерживает распределенные системы
 * - Нет встроенной сериализации метрик на диск
 */
class MetricsCollector {
public:
    /**
     * @brief Получить экземпляр MetricsCollector
     * @return Единственный экземпляр класса
     * 
     * @code
     * auto& metrics = MetricsCollector::instance();
     * @endcode
     */
    static MetricsCollector& instance();
    
    /**
     * @brief Зарегистрировать новый счетчик
     * @param name Уникальное имя счетчика
     * @param help Описание метрики (для Prometheus)
     * @throw std::runtime_error Если счетчик уже зарегистрирован
     * 
     * @note Имя должно соответствовать regex: [a-zA-Z_][a-zA-Z0-9_]*
     */
    void registerCounter(const std::string& name, const std::string& help = "");

    /**
     * @brief Увеличить значение счетчика
     * @param name Имя зарегистрированного счетчика
     * @param value Значение для инкремента (по умолчанию 1.0)
     * 
     * @warning Не вызывает исключений для незарегистрированных счетчиков
     */
    void incrementCounter(const std::string& name, double value = 1.0);
    
    /**
     * @brief Записать время выполнения задачи
     * @param name Имя метрики
     * @param duration Время выполнения в миллисекундах
     * 
     * @note Автоматически агрегирует значения для summary
     */
    void recordTaskTime(const std::string& name, std::chrono::milliseconds duration);
    
    /**
     * @brief Экспорт метрик в формате Prometheus
     * @return Строка с метриками в Prometheus text-based формате
     * 
     * @code
     * GET /metrics HTTP/1.1
     * Content-Type: text/plain; version=0.0.4
     * ...
     * @endcode
     */
    std::string exportPrometheus() const;

private:
    MetricsCollector() = default;
    
    /// Внутренняя структура для хранения метрик-счетчиков
    struct Metric {
        std::atomic<double> value{0.0}; ///< Текущее значение счетчика
        std::string help; ///< Описание для Prometheus
    };
    
    mutable std::mutex mutex_; ///< Мьютекс для потокобезопасности
    std::unordered_map<std::string, Metric> counters_; ///< Регистр счетчиков
    std::unordered_map<std::string, std::atomic<uint64_t>> taskTimes_; ///< Времена задач
};

} // namespace stc