/**
@file prometheus_exporter.cpp
@brief Реализация сериализатора метрик в формат Prometheus Text Exposition.
*/
#include "stc/metrics/prometheus_exporter.hpp"

#include <sstream>
#include <stdexcept>

#include "stc/metrics/metrics_registry.hpp"

namespace stc::metrics {

// ==============================================================================
// PrometheusExporter
// ==============================================================================

PrometheusExporter::PrometheusExporter(std::ostream& out,
                                       std::string_view namespace_prefix)
    : out_(out), namespace_prefix_(namespace_prefix) {}

std::string PrometheusExporter::GetFullName(std::string_view name) const {
  if (namespace_prefix_.empty()) {
    return std::string(name);
  }
  std::string result;
  result.reserve(namespace_prefix_.size() + 1 + name.size());
  result.append(namespace_prefix_);
  result.append("_");
  result.append(name);
  return result;
}

void PrometheusExporter::Visit(std::string_view name, std::string_view help,
                               const ICounter& counter) {
  // Приведение к конкретной реализации для доступа к атомарному значению
  const auto* atomic_counter = dynamic_cast<const AtomicCounter*>(&counter);
  if (!atomic_counter) return;  // Защита от NoOp или Mock-реализаций

  std::string base_name = GetFullName(name);

  // Prometheus требует суффикс _total для счетчиков
  std::string full_name = base_name;
  if (full_name.size() < 6 ||
      full_name.substr(full_name.size() - 6) != "_total") {
    full_name += "_total";
  }

  if (!help.empty()) {
    out_ << "# HELP " << full_name << " " << help << "\n";
  }
  out_ << "# TYPE " << full_name << " counter\n";
  out_ << full_name << " " << atomic_counter->GetValue() << "\n";
}

void PrometheusExporter::Visit(std::string_view name, std::string_view help,
                               const IGauge& gauge) {
  const auto* atomic_gauge = dynamic_cast<const AtomicGauge*>(&gauge);
  if (!atomic_gauge) return;

  std::string full_name = GetFullName(name);

  if (!help.empty()) {
    out_ << "# HELP " << full_name << " " << help << "\n";
  }
  out_ << "# TYPE " << full_name << " gauge\n";
  out_ << full_name << " " << atomic_gauge->GetValue() << "\n";
}

void PrometheusExporter::Visit(std::string_view name, std::string_view help,
                               const IHistogram& histogram) {
  const auto* atomic_hist = dynamic_cast<const AtomicHistogram*>(&histogram);
  if (!atomic_hist) return;

  std::string full_name = GetFullName(name);

  if (!help.empty()) {
    out_ << "# HELP " << full_name << " " << help << "\n";
  }
  out_ << "# TYPE " << full_name << " histogram\n";

  const auto& boundaries = atomic_hist->GetBoundaries();
  const auto* buckets = atomic_hist->GetBuckets();
  std::size_t boundary_count = boundaries.size();

  // Prometheus требует кумулятивные (накопительные) значения для бакетов
  uint64_t cumulative = 0;
  for (std::size_t i = 0; i < boundary_count; ++i) {
    cumulative += buckets[i].load(std::memory_order_relaxed);
    out_ << full_name << "_bucket{le=\"" << boundaries[i] << "\"} "
         << cumulative << "\n";
  }

  // Последний бакет +Inf всегда равен общему количеству наблюдений
  uint64_t total_count = atomic_hist->GetCount();
  out_ << full_name << "_bucket{le=\"+Inf\"} " << total_count << "\n";

  out_ << full_name << "_sum " << atomic_hist->GetSum() << "\n";
  out_ << full_name << "_count " << total_count << "\n";
}

// ==============================================================================
// Свободная функция-обертка
// ==============================================================================

std::string ExportToPrometheus(const IMetricsRegistry& registry,
                               std::string_view namespace_prefix) {
  std::ostringstream oss;
  PrometheusExporter exporter(oss, namespace_prefix);

  // Обход всех зарегистрированных метрик и их сериализация
  registry.AcceptVisitor(exporter);

  return oss.str();
}

}  // namespace stc::metrics