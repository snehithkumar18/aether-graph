#ifndef AETHER_GRAPH_OBJECT_POOL_H
#define AETHER_GRAPH_OBJECT_POOL_H

#include <vector>
#include <memory>
#include <mutex>

namespace AetherGraph {

template <typename T>
class ObjectPool {
private:
    std::vector<T*> pool_;
    std::vector<std::unique_ptr<T[]>> chunks_;
    size_t chunk_size_;
    std::mutex mutex_;

    void allocate_chunk() {
        auto chunk = std::make_unique<T[]>(chunk_size_);
        for (size_t i = 0; i < chunk_size_; ++i) {
            pool_.push_back(&chunk[i]);
        }
        chunks_.push_back(std::move(chunk));
    }

public:
    explicit ObjectPool(size_t chunk_size = 1024) : chunk_size_(chunk_size) {}
    ~ObjectPool() = default;

    T* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty()) {
            allocate_chunk();
        }
        T* obj = pool_.back();
        pool_.pop_back();
        return obj;
    }

    void release(T* obj) {
        if (!obj) return;
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(obj);
    }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_OBJECT_POOL_H
