#include "object_pool.h"
#include "graph_engine.h"

namespace AetherGraph {

// Dummy implementations to instantiate pools and compile object_pool.o translation unit
class NodePoolMetrics {
public:
    static size_t get_active_pools() {
        return 2;
    }
};

} // namespace AetherGraph
