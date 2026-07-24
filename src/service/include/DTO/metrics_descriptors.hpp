/**
@file metrics_descriptors.hpp
@brief Структуры данных (DTO) для группировки дескрипторов метрик.
@version 1.0.0
@date 2026-07-21
*/
#pragma once

#include <memory>
#include "stc/metrics/imetric.hpp"

namespace stc {

/**
@struct GlobalMetricsDescriptors
@brief Агрегирует дескрипторы общих (агрегированных) метрик сервиса.
*/
struct GlobalMetricsDescriptors {
    std::shared_ptr<stc::metrics::ICounter> files_processed;
    std::shared_ptr<stc::metrics::ICounter> files_failed;
    std::shared_ptr<stc::metrics::ICounter> records_processed;
    std::shared_ptr<stc::metrics::ICounter> records_matched;
    std::shared_ptr<stc::metrics::ICounter> bytes_processed;
    
    std::shared_ptr<stc::metrics::IGauge> templates_count;
    std::shared_ptr<stc::metrics::IGauge> active_workers;
    
    std::shared_ptr<stc::metrics::IHistogram> duration_hist;
    std::shared_ptr<stc::metrics::IGauge> duration_avg;
};

/**
@struct SourceMetricsDescriptors
@brief Агрегирует дескрипторы детализированных (пер-источниковых) метрик.
*/
struct SourceMetricsDescriptors {
    std::shared_ptr<stc::metrics::ICounter> files_processed;
    std::shared_ptr<stc::metrics::ICounter> files_failed;
    std::shared_ptr<stc::metrics::ICounter> files_failed_parse;
    std::shared_ptr<stc::metrics::ICounter> files_failed_write;
    
    std::shared_ptr<stc::metrics::ICounter> records_processed;
    std::shared_ptr<stc::metrics::ICounter> records_matched;
    std::shared_ptr<stc::metrics::ICounter> bytes_processed;
    
    std::shared_ptr<stc::metrics::IGauge> templates_count;
    std::shared_ptr<stc::metrics::IGauge> worker_state;
    std::shared_ptr<stc::metrics::IGauge> last_file_processed_timestamp;
    
    std::shared_ptr<stc::metrics::IHistogram> duration_hist;
    std::shared_ptr<stc::metrics::IGauge> duration_avg;
};

} // namespace stc