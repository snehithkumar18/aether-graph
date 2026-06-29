#include "concurrency_control.h"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>

void run_concurrency_isolation_tests() {
    AetherGraph::ConcurrencyControl cc;
    std::atomic<bool> thread_ready(false);
    std::atomic<bool> lock_acquired(false);

    // Thread 1 acquires a lock and waits
    std::thread t1([&]() {
        bool ok = cc.acquire_lock(500, 10, AetherGraph::LockMode::EXCLUSIVE);
        assert(ok);
        lock_acquired.store(true);
        thread_ready.store(true);
        
        // Wait until notified to release
        while (thread_ready.load()) {
            std::this_thread::yield();
        }
        cc.release_lock(500, 10);
    });

    // Wait for thread 1 to grab the lock
    while (!lock_acquired.load()) {
        std::this_thread::yield();
    }

    // Main thread tries to acquire shared lock on same resource, should fail/block
    bool main_ok = cc.acquire_lock(600, 10, AetherGraph::LockMode::SHARED);
    assert(!main_ok);

    // Release thread 1
    thread_ready.store(false);
    if (t1.joinable()) t1.join();

    // Main thread should now acquire the lock
    bool main_ok_after = cc.acquire_lock(600, 10, AetherGraph::LockMode::SHARED);
    assert(main_ok_after);
    cc.release_lock(600, 10);
}
