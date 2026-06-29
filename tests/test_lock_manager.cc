#include "lock_manager.h"
#include <cassert>
#include <iostream>

void run_lock_manager_tests() {
    AetherGraph::LockManager lm;

    // Acquire Shared Lock on key1
    bool ok1 = lm.acquire_shared(1, "key1");
    assert(ok1);

    // Concurrently acquire Shared Lock on key1 by transaction 2
    bool ok2 = lm.acquire_shared(2, "key1");
    assert(ok2);

    // Acquire Exclusive Lock on key1 by transaction 3 should fail
    bool ok3 = lm.acquire_exclusive(3, "key1");
    assert(!ok3);

    // Release shared locks
    lm.release(1, "key1");
    lm.release(2, "key1");

    // Acquire Exclusive Lock on key1 by transaction 3 should now succeed
    bool ok4 = lm.acquire_exclusive(3, "key1");
    assert(ok4);

    // Release exclusive lock
    lm.release(3, "key1");
}
