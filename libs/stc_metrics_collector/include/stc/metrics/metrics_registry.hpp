/**
@file metrics_registry.hpp
@brief Конкретная lock-free реализация реестра метрик и атомарных примитивов.
*/
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "imetric.hpp"
#include "imetrics_registry.hpp"

namespace stc::metrics {

/**
@class AtomicCounter
@brief Потокобезопасная lock-free реализация монотонно возрастающего счетчика.
*/
class AtomicCounter final : public ICounter {
 public:
  /**
  @brief Увеличивает значение счетчика.
  @param[in] value Значение для инкремента (должно быть >= 0).
  @throw std::invalid_argument Если value < 0.
  */
  void Increment(double value) override {
    if (value < 0.0) {
      throw std::invalid_argument(
          "Counter increment value must be non-negative");
    }
    value_.fetch_add(value, std::memory_order_relaxed);
  }

  /**
  @brief Возвращает текущее значение счетчика.
  @return double Текущее значение.
  */
  [[nodiscard]] double GetValue() const noexcept {
    return value_.load(std::memory_order_relaxed);
  }

 private:
  /// @private Атомарное хранилище значения.
  std::atomic<double> value_{0.0};
};

/**
@class AtomicGauge
@brief Потокобезопасная lock-free реализация датчика (значения, которое может
меняться в обе стороны).
*/
class AtomicGauge final : public IGauge {
 public:
  void Set(double value) override {
    value_.store(value, std::memory_order_relaxed);
  }

  void Increment(double value) override {
    value_.fetch_add(value, std::memory_order_relaxed);
  }

  void Decrement(double value) override {
    value_.fetch_sub(value, std::memory_order_relaxed);
  }

  /**
  @brief Возвращает текущее значение датчика.
  @return double Текущее значение.
  */
  [[nodiscard]] double GetValue() const noexcept {
    return value_.load(std::memory_order_relaxed);
  }

 private:
  /// @private Атомарное хранилище значения.
  std::atomic<double> value_{0.0};
};

/**
@class AtomicHistogram
@brief Потокобезопасная lock-free реализация гистограммы с предопределенными
бакетами.
*/
class AtomicHistogram final : public IHistogram {
 public:
  /**
  @brief Конструирует гистограмму с заданными границами бакетов.
  @param[in] boundaries Отсортированный по возрастанию массив границ бакетов.
  @throw std::invalid_argument Если массив пуст или не отсортирован.
  */
  explicit AtomicHistogram(std::vector<double> boundaries);

  /**
  @brief Добавляет новое наблюдение в гистограмму.
  @param[in] value Наблюдаемое значение. Валидация (например, проверка на NaN
  или Inf) намеренно не выполняется на «горячем пути» ради максимальной
  производительности. Ответственность за фильтрацию некорректных данных лежит на
  вызывающем коде.
  */
  void Observe(double value) override;

  /**
  @brief Возвращает массив границ бакетов.
  @return const std::vector<double>& Границы бакетов.
  */
  [[nodiscard]] const std::vector<double>& GetBoundaries() const noexcept {
    return boundaries_;
  }

  /**
  @brief Возвращает массив атомарных счетчиков бакетов.
  @return const std::atomic<uint64_t>* Указатель на массив бакетов.
  */
  [[nodiscard]] const std::atomic<uint64_t>* GetBuckets() const noexcept {
    return buckets_.get();
  }

  /**
  @brief Возвращает количество бакетов (размер массива boundaries + 1).
  @return std::size_t Количество бакетов.
  */
  [[nodiscard]] std::size_t GetBucketCount() const noexcept {
    return bucket_count_;
  }

  /**
  @brief Возвращает общее количество наблюдений.
  @return uint64_t Количество наблюдений.
  */
  [[nodiscard]] uint64_t GetCount() const noexcept {
    return count_.load(std::memory_order_relaxed);
  }

  /**
  @brief Возвращает сумму всех наблюдаемых значений.
  @return double Сумма значений.
  */
  [[nodiscard]] double GetSum() const noexcept {
    return sum_.load(std::memory_order_relaxed);
  }

 private:
  /// @private Границы бакетов (неизменяемы после конструирования).
  std::vector<double> boundaries_;

  /// @private Массив атомарных счетчиков для каждого бакета (размер =
  /// boundaries_.size() + 1).
  std::unique_ptr<std::atomic<uint64_t>[]> buckets_;

  /// @private Количество бакетов (кэшированное значение для быстродействия).
  std::size_t bucket_count_;

  /// @private Атомарный счетчик общего количества наблюдений.
  std::atomic<uint64_t> count_{0};

  /// @private Атомарная сумма всех наблюдаемых значений.
  std::atomic<double> sum_{0.0};
};

/**
@class MetricsRegistry
@brief Конкретная реализация реестра метрик. Обеспечивает фазу регистрации и
lock-free сбор.
*/
class MetricsRegistry final : public IMetricsRegistry {
 public:
  /**
  @brief Регистрирует новый счетчик.
  @param[in] name Уникальное имя метрики.
  @param[in] help Описание метрики для экспортера.
  @return std::shared_ptr<ICounter> Дескриптор для lock-free инкремента.
  @throw std::invalid_argument Если имя пустое или уже зарегистрировано.
  */
  std::shared_ptr<ICounter> RegisterCounter(std::string_view name,
                                            std::string_view help) override;

  /**
  @brief Регистрирует новый датчик.
  @param[in] name Уникальное имя метрики.
  @param[in] help Описание метрики для экспортера.
  @return std::shared_ptr<IGauge> Дескриптор для lock-free изменения значения.
  @throw std::invalid_argument Если имя пустое или уже зарегистрировано.
  */
  std::shared_ptr<IGauge> RegisterGauge(std::string_view name,
                                        std::string_view help) override;

  /**
  @brief Регистрирует новую гистограмму.
  @param[in] name Уникальное имя метрики.
  @param[in] help Описание метрики для экспортера.
  @param[in] buckets Массив границ бакетов (должен быть отсортирован по
  возрастанию).
  @return std::shared_ptr<IHistogram> Дескриптор для lock-free добавления
  наблюдений.
  @throw std::invalid_argument Если имя пустое, уже зарегистрировано, или массив
  бакетов невалиден.
  */
  std::shared_ptr<IHistogram> RegisterHistogram(
      std::string_view name, std::string_view help,
      std::vector<double> buckets) override;

  /**
  @brief Позволяет экспортеру обойти все зарегистрированные метрики.
  @param[in] visitor Экспортер, реализующий логику форматирования.
  */
  void AcceptVisitor(IExporterVisitor& visitor) const override;

 private:
  /**
  @private
  @struct MetricRecord
  @brief Внутренняя структура для связывания метаданных (имя, описание) с
  экземпляром метрики. Разделение необходимо для того, чтобы атомарные примитивы
  не хранили строковые данные, избегая аллокаций и накладных расходов на
  «горячем пути» (hot path).
  */
  struct MetricRecord {
    /// @private
    std::string name;  ///< Уникальное имя метрики, передаваемое в экспортер при
                       ///< сериализации.

    /// @private
    std::string
        help;  ///< Описание метрики (директива HELP), передаваемое в экспортер.

    /// @private
    std::shared_ptr<IMetric>
        instance;  ///< Умный указатель на экземпляр метрики, гарантирующий
                   ///< корректный жизненный цикл и потокобезопасный доступ.
  };

  /**
  @brief Проверяет уникальность имени метрики и регистрирует её.
  @param[in] name Имя метрики.
  @param[in] help Описание метрики.
  @param[in] metric Экземпляр метрики.
  @throw std::invalid_argument Если имя пустое или уже зарегистрировано.
  */
  void RegisterMetric(std::string_view name, std::string_view help,
                      std::shared_ptr<IMetric> metric);

  /// @private Хранилище зарегистрированных метрик (порядок важен для
  /// стабильного экспорта).
  std::vector<MetricRecord> records_;

  /// @private Хэш-таблица для O(1) проверки дубликатов на этапе инициализации.
  std::unordered_map<std::string, std::size_t> lookup_;
};

}  // namespace stc::metrics