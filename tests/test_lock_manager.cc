#include "lock_manager.h"
#include <cassert>
#include <iostream>
using namespace AetherGraph;


void run_lock_manager_tests() {
    AetherGraph::LockManager lm;

    // Acquire Shared Lock on 101
    bool ok1 = lm.acquire_shared(1, 101);
    assert(ok1);

    // Concurrently acquire Shared Lock on 101 by transaction 2
    bool ok2 = lm.acquire_shared(2, 101);
    assert(ok2);

    // Acquire Exclusive Lock on 101 by transaction 3 should fail
    bool ok3 = lm.acquire_exclusive(3, 101);
    assert(!ok3);

    // Release shared locks
    lm.release(1, 101);
    lm.release(2, 101);

    // Acquire Exclusive Lock on 101 by transaction 3 should now succeed
    bool ok4 = lm.acquire_exclusive(3, 101);
    assert(ok4);

    // Release exclusive lock
    lm.release(3, 101);
}
