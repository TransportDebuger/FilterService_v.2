/**
@file prometheus_exporter.hpp
@brief Сериализатор метрик в формат Prometheus Text Exposition.
*/
#pragma once

#include "imetric.hpp"

#include <ostream>
#include <string>
#include <string_view>

namespace stc::metrics {

class IMetricsRegistry; // Опережающее объявление

/**
@class PrometheusExporter
@brief Реализует паттерн Visitor для сериализации атомарных метрик в текстовый формат Prometheus.
*/
class PrometheusExporter final : public IExporterVisitor {
public:
    /**
    @brief Конструирует экспортер с заданным потоком вывода и префиксом.
    @param[in] out Поток вывода (например, std::ostringstream или буфер HTTP-ответа).
    @param[in] namespace_prefix Префикс неймспейса (например, "xmlfilter"). Если пуст, префикс не добавляется.
    */
    explicit PrometheusExporter(std::ostream& out, std::string_view namespace_prefix = "");

    /**
    @brief Посещает и сериализует счетчик.
    @param[in] name Имя метрики.
    @param[in] help Описание метрики.
    @param[in] counter Ссылка на счетчик.
    */
    void Visit(std::string_view name, std::string_view help, const ICounter& counter) override;

    /**
    @brief Посещает и сериализует датчик.
    @param[in] name Имя метрики.
    @param[in] help Описание метрики.
    @param[in] gauge Ссылка на датчик.
    */
    void Visit(std::string_view name, std::string_view help, const IGauge& gauge) override;

    /**
    @brief Посещает и сериализует гистограмму.
    @param[in] name Имя метрики.
    @param[in] help Описание метрики.
    @param[in] histogram Ссылка на гистограмму.
    */
    void Visit(std::string_view name, std::string_view help, const IHistogram& histogram) override;

private:
    /// @private Поток вывода для формирования текста.
    std::ostream& out_;
    
    /// @private Префикс неймспейса (добавляется к имени метрики через '_').
    std::string namespace_prefix_;
    
    /**
    @private
    @brief Формирует полное имя метрики с учетом префикса неймспейса.
    @param[in] name Базовое имя метрики.
    @return std::string Полное имя в формате "{prefix}_{name}" или "{name}", если префикс пуст.
    */
    [[nodiscard]] std::string GetFullName(std::string_view name) const;
};

/**
@brief Удобная обертка для экспорта всего реестра в строку.
@param[in] registry Реестр метрик.
@param[in] namespace_prefix Префикс неймспейса.
@return std::string Строка в формате Prometheus Text Exposition.
*/
[[nodiscard]] std::string ExportToPrometheus(const IMetricsRegistry& registry, std::string_view namespace_prefix = "");

} // namespace stc::metrics