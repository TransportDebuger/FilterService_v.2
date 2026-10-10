/**
 * @file metrics_descriptors.hpp
 * @brief Структуры данных (DTO) для группировки дескрипторов метрик.
 * @version 1.0.0
 * @date 2026-07-21
 */
#pragma once

#include <memory>

#include "stc/metrics/imetric.hpp"

namespace stc {

/**
 * @struct stc::GlobalMetricsDescriptors
 * @brief Агрегирует дескрипторы общих (агрегированных) метрик сервиса.
 */
struct GlobalMetricsDescriptors {
  /// @brief Общее количество успешно обработанных файлов.
  std::shared_ptr<stc::metrics::ICounter> files_processed;
  /// @brief Общее количество файлов, обработанных с ошибкой.
  std::shared_ptr<stc::metrics::ICounter> files_failed;
  /// @brief Общее количество обработанных записей (строк).
  std::shared_ptr<stc::metrics::ICounter> records_processed;
  /// @brief Общее количество записей, совпавших с фильтрами.
  std::shared_ptr<stc::metrics::ICounter> records_matched;
  /// @brief Общий объем обработанных данных в байтах.
  std::shared_ptr<stc::metrics::ICounter> bytes_processed;
  /// @brief Текущее количество загруженных шаблонов фильтрации.
  std::shared_ptr<stc::metrics::IGauge> templates_count;
  /// @brief Текущее количество активных рабочих потоков.
  std::shared_ptr<stc::metrics::IGauge> active_workers;
  /// @brief Гистограмма времени обработки одного файла.
  std::shared_ptr<stc::metrics::IHistogram> duration_hist;
  /// @brief Скользящее среднее время обработки файла.
  std::shared_ptr<stc::metrics::IGauge> duration_avg;
};

/**
 * @struct stc::SourceMetricsDescriptors
 * @brief Агрегирует дескрипторы детализированных (пер-источниковых) метрик.
 */
struct SourceMetricsDescriptors {
  /// @brief Количество файлов, обработанных для данного источника.
  std::shared_ptr<stc::metrics::ICounter> files_processed;
  /// @brief Количество файлов с ошибками для данного источника.
  std::shared_ptr<stc::metrics::ICounter> files_failed;
  /// @brief Количество файлов с ошибками парсинга (источник).
  std::shared_ptr<stc::metrics::ICounter> files_failed_parse;
  /// @brief Количество файлов с ошибками записи (источник).
  std::shared_ptr<stc::metrics::ICounter> files_failed_write;
  /// @brief Количество обработанных записей (источник).
  std::shared_ptr<stc::metrics::ICounter> records_processed;
  /// @brief Количество записей, совпавших с фильтрами (источник).
  std::shared_ptr<stc::metrics::ICounter> records_matched;
  /// @brief Объем обработанных данных в байтах (источник).
  std::shared_ptr<stc::metrics::ICounter> bytes_processed;
  /// @brief Текущее количество шаблонов для данного источника.
  std::shared_ptr<stc::metrics::IGauge> templates_count;
  /// @brief Текущее состояние рабочего потока (0 или 1).
  std::shared_ptr<stc::metrics::IGauge> worker_state;
  /// @brief Unix-время последней обработки файла (источник).
  std::shared_ptr<stc::metrics::IGauge> last_file_processed_timestamp;
  /// @brief Гистограмма времени обработки файла (источник).
  std::shared_ptr<stc::metrics::IHistogram> duration_hist;
  /// @brief Скользящее среднее время обработки (источник).
  std::shared_ptr<stc::metrics::IGauge> duration_avg;
};

}  // namespace stc