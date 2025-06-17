#include "stc/MetricsCollector.hpp"
#include <stdexcept>

namespace stc {

MetricsCollector& MetricsCollector::instance() {
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::registerCounter(const std::string& name, const std::string& help) {
    std::lock_guard lock(mutex_);
    if (counters_.count(name)) {
        throw std::runtime_error("Metric already registered: " + name);
    }
    counters_[name].help = help;
}

void MetricsCollector::incrementCounter(const std::string& name, double value) {
    std::lock_guard lock(mutex_);
    if (auto it = counters_.find(name); it != counters_.end()) {
        it->second.value.fetch_add(value, std::memory_order_relaxed);
    }
}

void MetricsCollector::recordTaskTime(const std::string& name, std::chrono::milliseconds duration) {
    taskTimes_[name].fetch_add(duration.count(), std::memory_order_relaxed);
}

std::string MetricsCollector::exportPrometheus() const {
    std::stringstream ss;
    std::lock_guard lock(mutex_);
    
    // Counters
    ss << "# TYPE metrics_collector_counter counter\n";
    for (const auto& [name, metric] : counters_) {
        if (!metric.help.empty()) {
            ss << "# HELP metrics_collector_" << name << " " << metric.help << "\n";
        }
        ss << "metrics_collector_" << name << " " 
           << metric.value.load(std::memory_order_relaxed) << "\n";
    }
    
    // Task times (summary)
    ss << "\n# TYPE metrics_collector_task_time summary\n";
    for (const auto& [name, value] : taskTimes_) {
        ss << "metrics_collector_task_time{quantile=\"0.5\"} " 
           << value.load(std::memory_order_relaxed) << "\n";
        ss << "metrics_collector_task_time_sum " 
           << value.load(std::memory_order_relaxed) << "\n";
        ss << "metrics_collector_task_time_count 1\n";
    }
    
    return ss.str();
}

} // namespace stc