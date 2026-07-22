/**
@file imetrics_registry.hpp
@brief Интерфейс реестра метрик.
*/
#pragma once

#include "imetric.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace stc::metrics {

/**
@class IMetricsRegistry
@brief Интерфейс фабрики и хранилища метрик.
*/
class IMetricsRegistry {
public:
    virtual ~IMetricsRegistry() = default;

    /**
    @brief Регистрирует новый счетчик.
    @param[in] name Уникальное имя метрики.
    @param[in] help Описание метрики для экспортера.
    @return std::shared_ptr<ICounter> Дескриптор для lock-free инкремента.
    @throw std::invalid_argument Если имя пустое или не соответствует стандарту.
    */
    virtual std::shared_ptr<ICounter> RegisterCounter(std::string_view name, std::string_view help) = 0;

    /**
    @brief Регистрирует новый датчик.
    @param[in] name Уникальное имя метрики.
    @param[in] help Описание метрики для экспортера.
    @return std::shared_ptr<IGauge> Дескриптор для lock-free изменения значения.
    */
    virtual std::shared_ptr<IGauge> RegisterGauge(std::string_view name, std::string_view help) = 0;

    /**
    @brief Регистрирует новую гистограмму.
    @param[in] name Уникальное имя метрики.
    @param[in] help Описание метрики для экспортера.
    @param[in] buckets Массив границ бакетов (должен быть отсортирован по возрастанию).
    @return std::shared_ptr<IHistogram> Дескриптор для lock-free добавления наблюдений.
    */
    virtual std::shared_ptr<IHistogram> RegisterHistogram(std::string_view name, std::string_view help, std::vector<double> buckets) = 0;

    /**
    @brief Позволяет экспортеру обойти все зарегистрированные метрики.
    @param[in] visitor Экспортер, реализующий логику форматирования.
    */
    virtual void AcceptVisitor(IExporterVisitor& visitor) const = 0;
};

} // namespace stc::metrics