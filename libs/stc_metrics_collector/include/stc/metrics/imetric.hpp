/**
@file imetric.hpp
@brief Базовые интерфейсы метрик и паттерн Visitor для сериализации.
*/
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace stc::metrics {

/// Опережающее объявление посетителя для разрыва циклических зависимостей
/// заголовков.
class IExporterVisitor;

/**
@class IMetric
@brief Базовый полиморфный интерфейс для любой метрики. Позволяет реестру
хранить разнородные метрики в едином контейнере.
*/
class IMetric {
 public:
  /// @brief Виртуальный деструктор для безопасного удаления производных классов
  /// через базовый указатель.
  virtual ~IMetric() = default;
};

/**
@class ICounter
@brief Интерфейс монотонно возрастающего счетчика.
*/
class ICounter : public IMetric {
 public:
  /**
  @brief Увеличивает значение счетчика.
  @param[in] value Значение для инкремента (должно быть >= 0).
  */
  virtual void Increment(double value = 1.0) = 0;
};

/**
@class IGauge
@brief Интерфейс метрики, значение которой может увеличиваться и уменьшаться.
*/
class IGauge : public IMetric {
 public:
  /**
  @brief Устанавливает абсолютное значение метрики.
  @param[in] value Новое значение.
  */
  virtual void Set(double value) = 0;

  /**
  @brief Увеличивает значение метрики.
  @param[in] value Значение для инкремента.
  */
  virtual void Increment(double value) = 0;

  /**
  @brief Уменьшает значение метрики.
  @param[in] value Значение для декремента.
  */
  virtual void Decrement(double value) = 0;
};

/**
@class IHistogram
@brief Интерфейс метрики для оценки распределения значений по бакетам.
*/
class IHistogram : public IMetric {
 public:
  /**
  @brief Добавляет новое наблюдение в гистограмму.
  @param[in] value Наблюдаемое значение.
  */
  virtual void Observe(double value) = 0;
};

/**
@class IExporterVisitor
@brief Интерфейс посетителя (экспортера) для сериализации метрик.
*/
class IExporterVisitor {
 public:
  virtual ~IExporterVisitor() = default;

  /**
  @brief Посещает счетчик.
  @param[in] name Имя метрики.
  @param[in] help Описание метрики.
  @param[in] counter Ссылка на счетчик.
  */
  virtual void Visit(std::string_view name, std::string_view help,
                     const ICounter& counter) = 0;

  /**
  @brief Посещает датчик.
  @param[in] name Имя метрики.
  @param[in] help Описание метрики.
  @param[in] gauge Ссылка на датчик.
  */
  virtual void Visit(std::string_view name, std::string_view help,
                     const IGauge& gauge) = 0;

  /**
  @brief Посещает гистограмму.
  @param[in] name Имя метрики.
  @param[in] help Описание метрики.
  @param[in] histogram Ссылка на гистограмму.
  */
  virtual void Visit(std::string_view name, std::string_view help,
                     const IHistogram& histogram) = 0;
};

}  // namespace stc::metrics