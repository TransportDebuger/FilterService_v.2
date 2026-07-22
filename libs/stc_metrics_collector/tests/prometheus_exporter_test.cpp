/**
@file prometheus_exporter_test.cpp
@brief Unit-тесты для сериализатора метрик в формат Prometheus Text Exposition.
*/
#include <gtest/gtest.h>

#include "stc/metrics/metrics_registry.hpp"
#include "stc/metrics/prometheus_exporter.hpp"
#include "stc/metrics/noop_metrics.hpp"

#include <string>

namespace stc::metrics::test {

// Вспомогательная функция для проверки наличия подстроки
bool Contains(const std::string& str, const std::string& sub) {
    return str.find(sub) != std::string::npos;
}

// ==============================================================================
// 1. Форматирование Counter
// ==============================================================================

TEST(PrometheusExporterTest, CounterFormattingAndTotalSuffix) {
    MetricsRegistry registry;
    auto counter = registry.RegisterCounter("requests", "Total requests");
    counter->Increment(42);

    std::string output = ExportToPrometheus(registry, "my_prefix");

    // Проверка HELP и TYPE
    EXPECT_TRUE(Contains(output, "# HELP my_prefix_requests_total Total requests\n"));
    EXPECT_TRUE(Contains(output, "# TYPE my_prefix_requests_total counter\n"));
    
    // Проверка значения и автоматического добавления суффикса _total
    EXPECT_TRUE(Contains(output, "my_prefix_requests_total 42\n"));
}

TEST(PrometheusExporterTest, CounterAlreadyHasTotalSuffix) {
    MetricsRegistry registry;
    // Имя уже содержит _total
    auto counter = registry.RegisterCounter("requests_total", "Total requests");
    counter->Increment(10);

    std::string output = ExportToPrometheus(registry, "");

    // Должно остаться requests_total, а не дублироваться в requests_total_total
    EXPECT_TRUE(Contains(output, "# TYPE requests_total counter\n"));
    EXPECT_TRUE(Contains(output, "requests_total 10\n"));
    EXPECT_FALSE(Contains(output, "requests_total_total"));
}

// ==============================================================================
// 2. Форматирование Gauge
// ==============================================================================

TEST(PrometheusExporterTest, GaugeFormatting) {
    MetricsRegistry registry;
    auto gauge = registry.RegisterGauge("active_connections", "Current connections");
    gauge->Set(15);

    std::string output = ExportToPrometheus(registry, "my_prefix");

    EXPECT_TRUE(Contains(output, "# HELP my_prefix_active_connections Current connections\n"));
    EXPECT_TRUE(Contains(output, "# TYPE my_prefix_active_connections gauge\n"));
    
    // Gauge не должен получать суффикс _total
    EXPECT_TRUE(Contains(output, "my_prefix_active_connections 15\n"));
    EXPECT_FALSE(Contains(output, "my_prefix_active_connections_total"));
}

// ==============================================================================
// 3. Форматирование Histogram (Кумулятивные бакеты)
// ==============================================================================

TEST(PrometheusExporterTest, HistogramFormattingAndCumulativeBuckets) {
    MetricsRegistry registry;
    // Границы: [1.0, 5.0, 10.0] -> Бакеты: [1.0, 5.0, 10.0, +Inf]
    auto hist = registry.RegisterHistogram("duration_seconds", "Time spent", {1.0, 5.0, 10.0});
    
    // Наблюдения:
    // 0.5 -> bucket 0 (<= 1.0)
    // 0.8 -> bucket 0 (<= 1.0)
    // 3.0 -> bucket 1 (<= 5.0)
    // 4.0 -> bucket 1 (<= 5.0)
    // 7.0 -> bucket 2 (<= 10.0)
    // 15.0 -> bucket 3 (+Inf)
    // 20.0 -> bucket 3 (+Inf)
    hist->Observe(0.5);
    hist->Observe(0.8);
    hist->Observe(3.0);
    hist->Observe(4.0);
    hist->Observe(7.0);
    hist->Observe(15.0);
    hist->Observe(20.0);

    std::string output = ExportToPrometheus(registry, "my_prefix");

    // Сумма: 0.5+0.8+3.0+4.0+7.0+15.0+20.0 = 50.3
    // Count: 7
    // Ожидаемые кумулятивные значения бакетов:
    // le="1"   -> 2 (0.5, 0.8)
    // le="5"   -> 4 (2 + 3.0, 4.0)
    // le="10"  -> 5 (4 + 7.0)
    // le="+Inf" -> 7 (5 + 15.0, 20.0)

    EXPECT_TRUE(Contains(output, "# HELP my_prefix_duration_seconds Time spent\n"));
    EXPECT_TRUE(Contains(output, "# TYPE my_prefix_duration_seconds histogram\n"));
    
    EXPECT_TRUE(Contains(output, "my_prefix_duration_seconds_bucket{le=\"1\"} 2\n"));
    EXPECT_TRUE(Contains(output, "my_prefix_duration_seconds_bucket{le=\"5\"} 4\n"));
    EXPECT_TRUE(Contains(output, "my_prefix_duration_seconds_bucket{le=\"10\"} 5\n"));
    EXPECT_TRUE(Contains(output, "my_prefix_duration_seconds_bucket{le=\"+Inf\"} 7\n"));
    
    EXPECT_TRUE(Contains(output, "my_prefix_duration_seconds_sum 50.3\n"));
    EXPECT_TRUE(Contains(output, "my_prefix_duration_seconds_count 7\n"));
}

// ==============================================================================
// 4. Обработка префикса неймспейса
// ==============================================================================

TEST(PrometheusExporterTest, EmptyNamespacePrefix) {
    MetricsRegistry registry;
    auto counter = registry.RegisterCounter("test_metric", "Help");
    counter->Increment(1);

    std::string output = ExportToPrometheus(registry, "");

    // При пустом префиксе имя должно начинаться сразу с метрики
    EXPECT_TRUE(Contains(output, "# TYPE test_metric_total counter\n"));
    EXPECT_TRUE(Contains(output, "test_metric_total 1\n"));
}

// ==============================================================================
// 5. Поведение с NoOp-заглушками
// ==============================================================================

TEST(PrometheusExporterTest, NoOpRegistryExportIsEmpty) {
    NoOpMetricsRegistry registry;
    registry.RegisterCounter("any", "any");
    registry.RegisterGauge("any2", "any2");
    
    std::string output = ExportToPrometheus(registry, "prefix");
    
    // NoOp метрики игнорируются экспортером (dynamic_cast к Atomic* вернет nullptr).
    // Следовательно, вывод должен быть полностью пустым, без мусора.
    EXPECT_TRUE(output.empty());
}

} // namespace stc::metrics::test