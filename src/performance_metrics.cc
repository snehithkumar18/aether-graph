#include "performance_metrics.h"
#include <algorithm>

namespace AetherGraph {

void PerformanceMetrics::record_value(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& counter = metrics_[name];
    counter.sum += value;
    counter.count++;
    counter.min_val = std::min(counter.min_val, value);
    counter.max_val = std::max(counter.max_val, value);
}

void PerformanceMetrics::reset_metric(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.erase(name);
}

void PerformanceMetrics::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

MetricSnapshot PerformanceMetrics::get_snapshot(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = metrics_.find(name);
    if (it == metrics_.end() || it->second.count == 0) {
        return {name, 0.0, 0.0, 0.0, 0};
    }
    
    const auto& c = it->second;
    return {name, c.min_val, c.max_val, c.sum / c.count, c.count};
}

std::vector<MetricSnapshot> PerformanceMetrics::get_all_snapshots() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MetricSnapshot> result;
    for (const auto& [name, c] : metrics_) {
        if (c.count > 0) {
            result.push_back({name, c.min_val, c.max_val, c.sum / c.count, c.count});
        }
    }
    return result;
}

} // namespace AetherGraph
