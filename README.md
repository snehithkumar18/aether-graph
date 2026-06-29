# AetherGraph

AetherGraph is a high-performance, transactional property graph database engine written in C++17. It supports Cypher-like query execution, Multi-Version Concurrency Control (MVCC) transactions, and a disk-backed storage engine.

## Purpose

AetherGraph is designed for applications requiring high-throughput graph traversals and concurrent read/write transactions. It combines the flexibility of a property graph model with the strict consistency guarantees of a transactional database.

Key Features:
* **Property Graph Model**: Nodes and edges can have rich, typed properties.
* **MVCC Transactions**: Multi-Version Concurrency Control allows readers to access snapshot data without blocking writers.
* **Two-Phase Locking (2PL)**: Shared and Exclusive locks on nodes ensure strict transaction isolation.
* **Cypher-like Query Language**: A declarative query parser and executor for node and edge creation, deletion, and path matching.
* **Disk-backed Storage & Buffer Pool**: Efficient paging and LRU cache eviction policies to manage memory.
* **Bloom Filters**: Fast edge existence checks to speed up path-finding traversals.

## Code Organization

The codebase is structured as follows:

* `src/`: The core source files of the database engine.
  * `utils.h`: Common type definitions (e.g., `txn_id_t`, `node_id_t`) and thread-safe logging.
  * `bloom_filter.h` / `bloom_filter.cc`: Probabilistic data structure for fast edge existence checks.
  * `lock_manager.h` / `lock_manager.cc`: Two-Phase Locking (2PL) implementation with shared and exclusive locks.
  * `storage.h` / `storage.cc`: Disk manager and LRU-based buffer pool manager.
  * `graph_engine.h` / `graph_engine.cc`: Graph representation (nodes, edges, properties, and custom adjacency list buckets).
  * `transaction_manager.h` / `transaction_manager.cc`: Transaction state tracking and undo log management for rollback.
  * `query_parser.h` / `query_parser.cc`: Parser for declarative query strings.
  * `query_executor.h` / `query_executor.cc`: Executor for query nodes, handling traversal algorithms (DFS and BFS).
* `fuzz/`: Fuzzing harnesses for testing robustness and memory safety.
  * `fuzz_query.cc`: Fuzzer for query parsing and execution.
  * `fuzz_transaction.cc`: Fuzzer for transaction concurrency and garbage collection.
* `.clusterfuzzlite/`: Configuration files for ClusterFuzzLite integration.
* `CMakeLists.txt`: Project build configuration.

## Building and Testing

To compile the project and its fuzzing targets:

```bash
mkdir build
cd build
cmake ..
make
```
