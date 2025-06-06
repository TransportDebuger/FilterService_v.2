#include "../include/metricscollector.hpp"
#include <sstream>

MetricsCollector& MetricsCollector::instance() {
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::registerCounter(const std::string& name) {
    std::lock_guard lock(mutex_);
    counters_.try_emplace(name, 0.0);
}

void MetricsCollector::incrementCounter(const std::string& name, double value) {
    std::lock_guard lock(mutex_);
    if (auto it = counters_.find(name); it != counters_.end()) {
        it->second.fetch_add(value, std::memory_order_relaxed);
    }
}

std::string MetricsCollector::exportPrometheus() const {
    std::stringstream ss;
    std::lock_guard lock(mutex_);
    
    ss << "# HELP metrics_collector Internal service metrics\n";
    ss << "# TYPE metrics_collector counter\n";
    
    for (const auto& [name, value] : counters_) {
        ss << "metrics_collector_" << name << " " 
           << value.load(std::memory_order_relaxed) << "\n";
    }
    
    return ss.str();
}