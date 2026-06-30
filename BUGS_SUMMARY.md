# AetherGraph Memory-Safety Bugs Summary

This document summarizes the memory-safety bugs implemented in the AetherGraph project for PROJECT FENRIR submission. These bugs are designed to be extremely difficult to detect and fix, targeting 0-3 solves out of 10 attempts by GPT-5.5.

## Bug List (12 bugs total)

### 1. Integer Overflow in deserialize_variant (graph_engine.cc)
**Location**: `src/graph_engine.cc:389-391`
**Type**: Out-of-bounds read via integer overflow
**Description**: The string deserialization reads a 16-bit length but has no upper bound check. If `len` is very large (e.g., 0xFFFF), the expression `offset + len` can overflow, passing the bounds check and causing an OOB read in the string constructor.
**Trigger**: Deserialize a variant with type STRING and length field set to 0xFFFF or similar large value.

### 2. Use-After-Free in trim_old_versions (graph_engine.cc)
**Location**: `src/graph_engine.cc:315-317`
**Type**: Use-after-free
**Description**: After freeing old PropertyVersion chain nodes, `last_kept->prev` is not nulled. If `resolve_property` is called later, it follows the stale `prev` pointer into freed memory.
**Trigger**: Perform multiple property updates on the same node to trigger version chain trimming, then call resolve_property.

### 3. Missing Bounds Check in Dijkstra Path Reconstruction (graph_algorithms.cc)
**Location**: `src/graph_algorithms.cc:146-149`
**Type**: Out-of-bounds map access
**Description**: The path reconstruction loop accesses `prev[curr]` without checking if `curr` exists in the map. If the graph has disconnected components or the reconstruction encounters a node not in `prev`, this causes undefined behavior.
**Trigger**: Run Dijkstra on a graph where the end node is not reachable from the start node.

### 4. Missing Bounds Check in Vector Index Search (vector_index.cc)
**Location**: `src/vector_index.cc:124-127`
**Type**: Out-of-bounds vector access
**Description**: The search function accesses `vectors_[idx]` without checking if `idx` is within bounds. If `cluster_buckets_` contains stale indices from previous `build_index` calls, this causes OOB access.
**Trigger**: Call `build_index` multiple times with different cluster counts, then perform a search.

### 5. Integer Overflow in Binary String Deserialization (graph_serializer.cc)
**Location**: `src/graph_serializer.cc:516-518`
**Type**: Out-of-bounds read via integer overflow
**Description**: Similar to bug #1, the binary string deserialization has no upper bound check on the 32-bit length field. Large values can cause integer overflow in the bounds check.
**Trigger**: Import binary data with a string length field set to a large value (e.g., 0xFFFFFFFF).

### 6. Integer Overflow in BFS Queue Growth (query_executor.cc)
**Location**: `src/query_executor.cc:66-73`
**Type**: Heap buffer overflow
**Description**: The BFS queue uses `uint8_t` for capacity. When capacity >= 128, `capacity * 2` overflows to 0 or a small value, allocating insufficient buffer and causing heap overflow.
**Trigger**: Create a graph with 128+ nodes reachable in BFS and trigger path checking.

### 7. Missing Bounds Check in B-tree Search (property_index.cc)
**Location**: `src/property_index.cc:96-99`
**Type**: Out-of-bounds unique_ptr access
**Description**: The recursive search accesses `node->children[i]` without checking if `i < node->children.size()`. If keys and children are desynchronized, this causes OOB access.
**Trigger**: Corrupt the B-tree structure or trigger incomplete split operations.

### 8. State Inconsistency in Buffer Pool Eviction (storage.cc)
**Location**: `src/storage.cc:117-120`
**Type**: Use-after-free
**Description**: If `lru_queue_` and `page_directory_` are desynchronized (victim_id not in directory), the page is removed from queue but not from directory. Future accesses may write/delete an already-freed page.
**Trigger**: Create race conditions or corruption in buffer pool management.

### 9. Type Confusion in Query Parser (query_parser.cc)
**Location**: `src/query_parser.cc:32-36`
**Type**: Type confusion
**Description**: `std::stoi` throws `std::out_of_range` for numbers larger than INT_MAX. The catch block treats these as strings instead of numbers, causing type mismatch bugs later.
**Trigger**: Parse a query with a numeric property value larger than INT_MAX.

### 10. Missing Escape Sequence Handling in Cypher Parser (cypher_parser.cc)
**Location**: `src/cypher_parser.cc:43-46`
**Type**: Malformed tokens / potential buffer overflow
**Description**: The string literal parser doesn't handle escape sequences. Escaped quotes (e.g., `\"` or `\'`) are treated as end-of-string, causing malformed tokens.
**Trigger**: Parse a Cypher query with escaped quotes in string literals.

### 11. Missing Upper Bound Check in WAL Recovery (wal_manager.cc)
**Location**: `src/wal_manager.cc:154-158`
**Type**: Heap buffer overflow / denial of service
**Description**: The WAL recovery reads a 32-bit length and resizes a string without an upper bound check. Very large values can cause huge allocations or OOB reads.
**Trigger**: Corrupt WAL file with large string length fields.

### 12. Missing Upper Bound Check in Histogram Resize (graph_analytics.cc)
**Location**: `src/graph_analytics.cc:66-69`
**Type**: Allocation failure / integer overflow
**Description**: The degree histogram resize uses `max_degree + 1` without bounds checking. If `max_degree` is very large (e.g., UINT32_MAX), this causes allocation failure or allocator overflow.
**Trigger**: Create a node with an extremely high degree (many outgoing edges).

## Existing Bugs (Preserved)

### 13. Double-Free in StorageCompressor (storage_compressor.h)
**Location**: `src/storage_compressor.h:28-33`
**Type**: Double-free
**Description**: The destructor deletes `root_`, but `build_tree()` already freed the tree via `free_tree()`. The `root_` pointer is not nulled, causing double-free.

### 14. Orphaned PropertyVersion Heads (transaction_manager.cc)
**Location**: `src/transaction_manager.cc:38-41`
**Type**: Use-after-free
**Description**: When aborting a property creation, the version head is not cleared from `node->version_heads`, leaving a dangling pointer.

### 15. Unlinked Aborted Versions (transaction_manager.cc)
**Location**: `src/transaction_manager.cc:44`
**Type**: Use-after-free / data corruption
**Description**: When aborting a property update, the aborted version is not unlinked from the version chain.

## Fuzzer Coverage

The fuzzers have been updated to reach these bugs:
- `fuzz_query.cc`: Covers deserialization, BFS, JIT, slotted pages, WAL, compressor, binary import, property index, graph analytics, and Cypher parser.
- `fuzz_transaction.cc`: Covers vector index, graph algorithms, and buffer pool eviction.

## Seed Corpus

Additional seed files have been added to help reach the bugs:
- `seed4.bin`, `seed5.bin`: Binary data for integer overflow bugs
- `seed6.txt`, `seed7.txt`, `seed8.txt`: Query strings for parser bugs

## Difficulty Characteristics

These bugs are designed to be hard because:
1. **Subtle**: Most are missing bounds checks or state inconsistencies, not obvious logic errors.
2. **State-dependent**: Many require specific sequences of operations to trigger (e.g., multiple updates, rebuilds).
3. **Deep**: Bugs are in complex subsystems (MVCC, indexing, serialization) requiring deep understanding.
4. **No obvious symptoms**: Some may not crash immediately, only under specific conditions.
5. **Hard to debug**: UAF bugs may only manifest with specific memory layouts or ASAN configurations.
