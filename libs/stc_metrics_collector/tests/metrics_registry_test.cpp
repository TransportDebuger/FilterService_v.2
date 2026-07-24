/**
@file metrics_registry_test.cpp
@brief Unit-тесты для ядра библиотеки stc::metrics (реестр, атомарные примитивы,
NoOp).
*/
#include "stc/metrics/metrics_registry.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

#include "stc/metrics/noop_metrics.hpp"

namespace stc::metrics::test {

// ==============================================================================
// 1. Верификация реестра и Defensive Programming
// ==============================================================================

TEST(MetricsRegistryTest, RegistersCounterAndGaugeSuccessfully) {
  MetricsRegistry registry;

  auto counter = registry.RegisterCounter("requests_total", "Total requests");
  auto gauge =
      registry.RegisterGauge("active_connections", "Current connections");

  ASSERT_NE(counter, nullptr);
  ASSERT_NE(gauge, nullptr);
}

TEST(MetricsRegistryTest, ThrowsOnDuplicateRegistration) {
  MetricsRegistry registry;
  registry.RegisterCounter("metric_a", "Help A");

  EXPECT_THROW(registry.RegisterCounter("metric_a", "Duplicate"),
               std::invalid_argument);
}

TEST(MetricsRegistryTest, ThrowsOnEmptyMetricName) {
  MetricsRegistry registry;

  EXPECT_THROW(registry.RegisterCounter("", "Empty name"),
               std::invalid_argument);
}

TEST(MetricsRegistryTest, HistogramValidation) {
  MetricsRegistry registry;

  // Пустой массив границ
  EXPECT_THROW(registry.RegisterHistogram("hist_empty", "Help", {}),
               std::invalid_argument);

  // Неотсортированный массив границ
  EXPECT_THROW(
      registry.RegisterHistogram("hist_unsorted", "Help", {10.0, 5.0, 20.0}),
      std::invalid_argument);

  // Успешная регистрация с валидными границами
  EXPECT_NO_THROW(
      registry.RegisterHistogram("hist_valid", "Help", {1.0, 5.0, 10.0}));
}

// ==============================================================================
// 2. Верификация атомарных примитивов (Базовая логика)
// ==============================================================================

TEST(AtomicPrimitivesTest, CounterIncrementAndValue) {
  MetricsRegistry registry;
  auto counter = std::dynamic_pointer_cast<AtomicCounter>(
      registry.RegisterCounter("test_counter", "Help"));

  ASSERT_NE(counter, nullptr);

  counter->Increment(5.0);
  counter->Increment(2.5);

  EXPECT_DOUBLE_EQ(counter->GetValue(), 7.5);

  // Counter не должен принимать отрицательные значения
  EXPECT_THROW(counter->Increment(-1.0), std::invalid_argument);
}

TEST(AtomicPrimitivesTest, GaugeSetIncrementDecrement) {
  MetricsRegistry registry;
  auto gauge = std::dynamic_pointer_cast<AtomicGauge>(
      registry.RegisterGauge("test_gauge", "Help"));

  ASSERT_NE(gauge, nullptr);

  gauge->Set(100.0);
  EXPECT_DOUBLE_EQ(gauge->GetValue(), 100.0);

  gauge->Increment(25.0);
  EXPECT_DOUBLE_EQ(gauge->GetValue(), 125.0);

  gauge->Decrement(50.0);
  EXPECT_DOUBLE_EQ(gauge->GetValue(), 75.0);
}

TEST(AtomicPrimitivesTest, HistogramObserveAndBuckets) {
  MetricsRegistry registry;
  // Границы: [10.0, 50.0, 100.0] -> Бакеты: [10.0, 50.0, 100.0, +Inf]
  auto hist = std::dynamic_pointer_cast<AtomicHistogram>(
      registry.RegisterHistogram("test_hist", "Help", {10.0, 50.0, 100.0}));

  ASSERT_NE(hist, nullptr);
  EXPECT_EQ(hist->GetBucketCount(), 4);  // 3 границы + 1 для +Inf

  // Наблюдения:
  // 5.0   -> попадает в бакет 0 (<= 10.0)
  // 25.0  -> попадает в бакет 1 (<= 50.0)
  // 75.0  -> попадает в бакет 2 (<= 100.0)
  // 150.0 -> попадает в бакет 3 (+Inf)
  // 8.0   -> попадает в бакет 0 (<= 10.0)
  hist->Observe(5.0);
  hist->Observe(25.0);
  hist->Observe(75.0);
  hist->Observe(150.0);
  hist->Observe(8.0);

  EXPECT_EQ(hist->GetCount(), 5);
  EXPECT_DOUBLE_EQ(hist->GetSum(), 263.0);  // 5+25+75+150+8

  const auto* buckets = hist->GetBuckets();
  // Проверка независимых счетчиков бакетов
  EXPECT_EQ(buckets[0].load(), 2);  // 5.0, 8.0
  EXPECT_EQ(buckets[1].load(), 1);  // 25.0
  EXPECT_EQ(buckets[2].load(), 1);  // 75.0
  EXPECT_EQ(buckets[3].load(), 1);  // 150.0
}

// ==============================================================================
// 3. Многопоточность и Lock-Free гарантии (Concurrent Test)
// ==============================================================================

TEST(ConcurrencyTest, NoDataRaceOnCounter) {
  MetricsRegistry registry;
  auto counter = std::dynamic_pointer_cast<AtomicCounter>(
      registry.RegisterCounter("concurrent_counter", "Help"));

  constexpr int kThreads = 8;
  constexpr int kIterations = 10000;

  std::vector<std::jthread> threads;
  threads.reserve(kThreads);

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&counter]() {
      for (int j = 0; j < kIterations; ++j) {
        counter->Increment(1.0);
      }
    });
  }

  // std::jthread автоматически вызывает join() в деструкторе
  threads.clear();

  EXPECT_DOUBLE_EQ(counter->GetValue(), kThreads * kIterations);
}

TEST(ConcurrencyTest, NoDataRaceOnHistogram) {
  MetricsRegistry registry;
  auto hist = std::dynamic_pointer_cast<AtomicHistogram>(
      registry.RegisterHistogram("concurrent_hist", "Help", {1.0, 5.0, 10.0}));

  constexpr int kThreads = 8;
  constexpr int kIterations = 5000;

  std::vector<std::jthread> threads;
  threads.reserve(kThreads);

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&hist]() {
      for (int j = 0; j < kIterations; ++j) {
        // Распределяем наблюдения по всем бакетам
        hist->Observe(0.5);   // bucket 0
        hist->Observe(3.0);   // bucket 1
        hist->Observe(7.0);   // bucket 2
        hist->Observe(15.0);  // bucket 3 (+Inf)
      }
    });
  }

  threads.clear();

  constexpr uint64_t kExpectedCount =
      kThreads * kIterations * 4;  // 4 observe per iteration
  constexpr double kExpectedSum =
      kThreads * kIterations * (0.5 + 3.0 + 7.0 + 15.0);

  EXPECT_EQ(hist->GetCount(), kExpectedCount);
  EXPECT_DOUBLE_EQ(hist->GetSum(), kExpectedSum);

  const auto* buckets = hist->GetBuckets();
  uint64_t expected_per_bucket = kThreads * kIterations;
  EXPECT_EQ(buckets[0].load(), expected_per_bucket);
  EXPECT_EQ(buckets[1].load(), expected_per_bucket);
  EXPECT_EQ(buckets[2].load(), expected_per_bucket);
  EXPECT_EQ(buckets[3].load(), expected_per_bucket);
}

// ==============================================================================
// 4. Верификация NoOp реализаций
// ==============================================================================

TEST(NoOpRegistryTest, OperationsDoNotThrowAndReturnValidPointers) {
  NoOpMetricsRegistry registry;

  auto counter = registry.RegisterCounter("any_name", "any_help");
  auto gauge = registry.RegisterGauge("any_name", "any_help");
  auto hist = registry.RegisterHistogram("any_name", "any_help", {1.0, 2.0});

  ASSERT_NE(counter, nullptr);
  ASSERT_NE(gauge, nullptr);
  ASSERT_NE(hist, nullptr);

  // Вызовы методов не должны вызывать падений или исключений
  EXPECT_NO_THROW(counter->Increment(100.0));
  EXPECT_NO_THROW(gauge->Set(50.0));
  EXPECT_NO_THROW(gauge->Increment(10.0));
  EXPECT_NO_THROW(gauge->Decrement(5.0));
  EXPECT_NO_THROW(hist->Observe(1.5));

  // AcceptVisitor также должен быть безопасным
  // (Создаем мок-посетитель на лету через лямбду/наследование, но для NoOp
  // достаточно просто вызвать AcceptVisitor с любым валидным объектом)
  struct DummyVisitor : public IExporterVisitor {
    void Visit(std::string_view, std::string_view, const ICounter&) override {}
    void Visit(std::string_view, std::string_view, const IGauge&) override {}
    void Visit(std::string_view, std::string_view, const IHistogram&) override {
    }
  };

  DummyVisitor visitor;
  EXPECT_NO_THROW(registry.AcceptVisitor(visitor));
}

}  // namespace stc::metrics::test