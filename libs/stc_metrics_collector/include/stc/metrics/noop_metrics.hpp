/**
@file noop_metrics.hpp
@brief No-Op (заглушки) реализации интерфейсов метрик и реестра для полного
отключения сбора.
*/
#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "imetric.hpp"
#include "imetrics_registry.hpp"

namespace stc::metrics {

/**
@class NoOpCounter
@brief Пустая реализация счетчика. Все вызовы инкремента игнорируются.
*/
class NoOpCounter final : public ICounter {
 public:
  void Increment(double value = 1.0) override {}
};

/**
@class NoOpGauge
@brief Пустая реализация датчика. Все вызовы изменения значения игнорируются.
*/
class NoOpGauge final : public IGauge {
 public:
  void Set(double value) override {}
  void Increment(double value) override {}
  void Decrement(double value) override {}
};

/**
@class NoOpHistogram
@brief Пустая реализация гистограммы. Все наблюдения игнорируются.
*/
class NoOpHistogram final : public IHistogram {
 public:
  void Observe(double value) override {}
};

/**
@class NoOpMetricsRegistry
@brief Пустая реализация реестра метрик. Возвращает No-Op дескрипторы без
аллокаций на горячем пути.
*/
class NoOpMetricsRegistry final : public IMetricsRegistry {
 public:
  /**
  @brief Конструирует реестр и инициализирует переиспользуемые No-Op
  дескрипторы.
  */
  NoOpMetricsRegistry()
      : noop_counter_(std::make_shared<NoOpCounter>()),
        noop_gauge_(std::make_shared<NoOpGauge>()),
        noop_histogram_(std::make_shared<NoOpHistogram>()) {}

  std::shared_ptr<ICounter> RegisterCounter(std::string_view name,
                                            std::string_view help) override {
    return noop_counter_;
  }

  std::shared_ptr<IGauge> RegisterGauge(std::string_view name,
                                        std::string_view help) override {
    return noop_gauge_;
  }

  std::shared_ptr<IHistogram> RegisterHistogram(
      std::string_view name, std::string_view help,
      std::vector<double> buckets) override {
    return noop_histogram_;
  }

  void AcceptVisitor(IExporterVisitor& visitor) const override {}

 private:
  /// @private Переиспользуемый дескриптор пустого счетчика (избегает аллокаций
  /// при регистрации).
  std::shared_ptr<ICounter> noop_counter_;

  /// @private Переиспользуемый дескриптор пустого датчика.
  std::shared_ptr<IGauge> noop_gauge_;

  /// @private Переиспользуемый дескриптор пустой гистограммы.
  std::shared_ptr<IHistogram> noop_histogram_;
};

}  // namespace stc::metrics