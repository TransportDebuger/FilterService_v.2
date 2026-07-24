/**
@file metrics_registry.cpp
@brief Реализация lock-free атомарных примитивов и реестра метрик.
*/
#include "stc/metrics/metrics_registry.hpp"

#include <algorithm>
#include <stdexcept>

namespace stc::metrics {

// ==============================================================================
// AtomicHistogram
// ==============================================================================

AtomicHistogram::AtomicHistogram(std::vector<double> boundaries)
    : boundaries_(std::move(boundaries)),
      bucket_count_(boundaries_.size() + 1) {
  if (boundaries_.empty()) {
    throw std::invalid_argument("Histogram boundaries must not be empty");
  }

  // Строгая проверка сортировки границ (Defensive Programming)
  for (std::size_t i = 1; i < boundaries_.size(); ++i) {
    if (boundaries_[i] <= boundaries_[i - 1]) {
      throw std::invalid_argument(
          "Histogram boundaries must be strictly sorted in ascending order");
    }
  }

  // Инициализация массива атомарных бакетов
  buckets_ = std::make_unique<std::atomic<uint64_t>[]>(bucket_count_);
  for (std::size_t i = 0; i < bucket_count_; ++i) {
    buckets_[i].store(0, std::memory_order_relaxed);
  }
}

void AtomicHistogram::Observe(double value) {
  // Бинарный поиск для нахождения первого бакета, граница которого >= value.
  // Если value больше всех границ, it укажет на end(), что соответствует
  // последнему бакету (+Inf).
  auto it = std::upper_bound(boundaries_.begin(), boundaries_.end(), value);
  std::size_t index = std::distance(boundaries_.begin(), it);

  // Lock-free инкременты
  buckets_[index].fetch_add(1, std::memory_order_relaxed);
  count_.fetch_add(1, std::memory_order_relaxed);
  sum_.fetch_add(value, std::memory_order_relaxed);
}

// ==============================================================================
// MetricsRegistry
// ==============================================================================

void MetricsRegistry::RegisterMetric(std::string_view name,
                                     std::string_view help,
                                     std::shared_ptr<IMetric> metric) {
  if (name.empty()) {
    throw std::invalid_argument("Metric name must not be empty");
  }

  std::string name_str(name);

  // Проверка на дубликаты (O(1) благодаря хэш-таблице lookup_)
  if (lookup_.count(name_str)) {
    throw std::invalid_argument("Metric already registered: " + name_str);
  }

  // Сохранение метаданных и экземпляра.
  // Эта операция происходит только на фазе инициализации (в одном потоке).
  std::size_t index = records_.size();
  records_.push_back({name_str, std::string(help), std::move(metric)});
  lookup_[name_str] = index;
}

std::shared_ptr<ICounter> MetricsRegistry::RegisterCounter(
    std::string_view name, std::string_view help) {
  auto counter = std::make_shared<AtomicCounter>();
  RegisterMetric(name, help, counter);
  return counter;
}

std::shared_ptr<IGauge> MetricsRegistry::RegisterGauge(std::string_view name,
                                                       std::string_view help) {
  auto gauge = std::make_shared<AtomicGauge>();
  RegisterMetric(name, help, gauge);
  return gauge;
}

std::shared_ptr<IHistogram> MetricsRegistry::RegisterHistogram(
    std::string_view name, std::string_view help, std::vector<double> buckets) {
  auto histogram = std::make_shared<AtomicHistogram>(std::move(buckets));
  RegisterMetric(name, help, histogram);
  return histogram;
}

void MetricsRegistry::AcceptVisitor(IExporterVisitor& visitor) const {
  // Обход зарегистрированных метрик для экспорта.
  // Используем dynamic_cast для разделения метаданных (name, help) и атомарных
  // данных. Это избавляет от необходимости дублировать строки внутри каждой
  // атомарной метрики.
  for (const auto& record : records_) {
    if (auto counter = dynamic_cast<const ICounter*>(record.instance.get())) {
      visitor.Visit(record.name, record.help, *counter);
    } else if (auto gauge =
                   dynamic_cast<const IGauge*>(record.instance.get())) {
      visitor.Visit(record.name, record.help, *gauge);
    } else if (auto histogram =
                   dynamic_cast<const IHistogram*>(record.instance.get())) {
      visitor.Visit(record.name, record.help, *histogram);
    }
  }
}

}  // namespace stc::metrics