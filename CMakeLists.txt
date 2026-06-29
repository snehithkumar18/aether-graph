cmake_minimum_required(VERSION 3.15)
project(AetherGraph CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings for code quality
add_compile_options(-Wall -Wextra)

# Source files
set(SRC_FILES
    src/bloom_filter.cc
    src/lock_manager.cc
    src/storage.cc
    src/graph_engine.cc
    src/transaction_manager.cc
    src/query_parser.cc
    src/query_executor.cc
    src/graph_algorithms.cc
    src/graph_serializer.cc
    src/graph_analytics.cc
    src/wal_manager.cc
    src/property_index.cc
    src/cypher_parser.cc
    src/concurrency_control.cc
    src/graph_statistics.cc
    src/query_optimizer.cc
    src/vector_index.cc
    src/metadata_manager.cc
    src/execution_plans.cc
    src/query_engine.cc
    src/schema_validator.cc
    src/graph_projection.cc
    src/object_pool.cc
    src/graph_server.cc
    src/thread_pool.cc
    src/graph_partitioner.cc
    src/disk_storage.cc
    src/graph_coordinator.cc
    src/query_compiler.cc
    src/backup_utility.cc
    src/performance_metrics.cc
    src/distributed_txn.cc
    src/query_compiler_jit.cc
    src/query_planner_cbo.cc
    src/query_engine_compiler.cc
    src/lock_manager_advanced.cc
    src/vector_index_hnsw.cc
    src/transaction_manager_savepoint.cc
    src/storage_compressor.cc
    src/query_planner_decorrelate.cc
    src/vectorized_executor.cc
)

# Core library
add_library(aether_graph_static STATIC ${SRC_FILES})
target_include_directories(aether_graph_static PUBLIC src)

# Fuzzing build setup
if(DEFINED ENV{LIB_FUZZING_ENGINE})
    # Under ClusterFuzzLite, the engine flags are provided by the environment
    set(FUZZER_LINK_LIBS $ENV{LIB_FUZZING_ENGINE})
else()
    # Local fallback for compilation tests
    set(FUZZER_LINK_LIBS "-fsanitize=fuzzer")
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined)
    link_libraries(-fsanitize=address,undefined)
endif()

# Fuzzing targets
add_executable(fuzz_query fuzz/fuzz_query.cc)
target_link_libraries(fuzz_query PRIVATE aether_graph_static ${FUZZER_LINK_LIBS})

add_executable(fuzz_transaction fuzz/fuzz_transaction.cc)
target_link_libraries(fuzz_transaction PRIVATE aether_graph_static ${FUZZER_LINK_LIBS})

# Unit tests runner
add_executable(run_tests
    tests/test_runner.cc
    tests/test_graph_algorithms.cc
    tests/test_graph_serializer.cc
    tests/test_graph_analytics.cc
    tests/test_concurrency_and_wal.cc
    tests/test_query_execution.cc
    tests/test_performance.cc
    tests/test_stress.cc
    tests/test_advanced_queries.cc
    tests/test_validation_and_projection.cc
    tests/test_storage_and_partitioning.cc
    tests/test_bloom_filter.cc
    tests/test_lock_manager.cc
    tests/test_query_parser.cc
    tests/test_metadata_manager.cc
    tests/test_bloom_filter_advanced.cc
    tests/test_disk_storage_stress.cc
    tests/test_concurrency_isolation.cc
    tests/test_coordinator_and_backup.cc
    tests/test_distributed.cc
    tests/test_performance_advanced.cc
    tests/test_performance_stress_queries.cc
    tests/test_metadata_catalog_advanced.cc
)
target_link_libraries(run_tests PRIVATE aether_graph_static)
