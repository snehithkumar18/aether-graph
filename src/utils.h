#ifndef AETHER_GRAPH_UTILS_H
#define AETHER_GRAPH_UTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>

namespace AetherGraph {

using txn_id_t = uint64_t;
using node_id_t = uint32_t;
using edge_id_t = uint64_t;
using lsn_t = uint64_t;

inline constexpr node_id_t INVALID_NODE_ID = 0;
inline constexpr txn_id_t INVALID_TXN_ID = 0;

class Logger {
public:
    static Logger& get_instance() {
        static Logger instance;
        return instance;
    }

    void info(const std::string& component, const std::string& msg) {
        log("INFO", component, msg);
    }

    void warn(const std::string& component, const std::string& msg) {
        log("WARN", component, msg);
    }

    void error(const std::string& component, const std::string& msg) {
        log("ERROR", component, msg);
    }

private:
    std::mutex mutex_;
    Logger() = default;

    void log(const std::string& level, const std::string& component, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cerr << "[" << level << "][" << component << "] " << msg << std::endl;
    }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_UTILS_H
