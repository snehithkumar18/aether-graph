#ifndef AETHER_GRAPH_PERFORMANCE_METRICS_H
#define AETHER_GRAPH_PERFORMANCE_METRICS_H

#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

namespace AetherGraph {

struct MetricSnapshot {
    std::string name;
    double min_val;
    double max_val;
    double average;
    size_t count;
};

class PerformanceMetrics {
private:
    struct MetricCounter {
        double sum = 0.0;
        double min_val = 9999999.0;
        double max_val = -9999999.0;
        size_t count = 0;
    };

    std::unordered_map<std::string, MetricCounter> metrics_;
    std::mutex mutex_;

public:
    PerformanceMetrics() = default;
    ~PerformanceMetrics() = default;

    void record_value(const std::string& name, double value);
    void reset_metric(const std::string& name);
    void reset_all();

    MetricSnapshot get_snapshot(const std::string& name);
    std::vector<MetricSnapshot> get_all_snapshots();
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_PERFORMANCE_METRICS_H
