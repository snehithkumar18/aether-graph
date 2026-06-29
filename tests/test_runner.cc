#include <iostream>
#include <vector>
#include <string>

// Test suites declarations
void run_graph_algorithms_tests();
void run_graph_serializer_tests();
void run_graph_analytics_tests();
void run_concurrency_and_wal_tests();
void run_query_execution_tests();
void run_performance_benchmarks();
void run_transaction_stress_tests();
void run_advanced_query_tests();
void run_validation_and_projection_tests();
void run_storage_and_partitioning_tests();
void run_bloom_filter_tests();
void run_lock_manager_tests();
void run_query_parser_tests();
void run_metadata_manager_tests();
void run_bloom_filter_advanced_tests();
void run_disk_storage_stress_tests();
void run_concurrency_isolation_tests();
void run_coordinator_and_backup_tests();
void run_distributed_tests();
void run_advanced_performance_benchmarks();
void run_performance_stress_queries_tests();

int main() {
    std::cout << "==================================================\n";
    std::cout << "           AETHERGRAPH TEST RUNNER                \n";
    std::cout << "==================================================\n";

    try {
        std::cout << "\n[RUNNING] Bloom Filter Tests...\n";
        run_bloom_filter_tests();
        std::cout << "[PASSED] Bloom Filter Tests.\n";

        std::cout << "\n[RUNNING] Advanced Bloom Filter Tests...\n";
        run_bloom_filter_advanced_tests();
        std::cout << "[PASSED] Advanced Bloom Filter Tests.\n";

        std::cout << "\n[RUNNING] Lock Manager Tests...\n";
        run_lock_manager_tests();
        std::cout << "[PASSED] Lock Manager Tests.\n";

        std::cout << "\n[RUNNING] Concurrency Isolation Tests...\n";
        run_concurrency_isolation_tests();
        std::cout << "[PASSED] Concurrency Isolation Tests.\n";

        std::cout << "\n[RUNNING] Query Parser Tests...\n";
        run_query_parser_tests();
        std::cout << "[PASSED] Query Parser Tests.\n";

        std::cout << "\n[RUNNING] Metadata Manager Tests...\n";
        run_metadata_manager_tests();
        std::cout << "[PASSED] Metadata Manager Tests.\n";

        std::cout << "\n[RUNNING] Graph Algorithms Tests...\n";
        run_graph_algorithms_tests();
        std::cout << "[PASSED] Graph Algorithms Tests.\n";

        std::cout << "\n[RUNNING] Graph Serializer Tests...\n";
        run_graph_serializer_tests();
        std::cout << "[PASSED] Graph Serializer Tests.\n";

        std::cout << "\n[RUNNING] Graph Analytics Tests...\n";
        run_graph_analytics_tests();
        std::cout << "[PASSED] Graph Analytics Tests.\n";

        std::cout << "\n[RUNNING] Concurrency and WAL Tests...\n";
        run_concurrency_and_wal_tests();
        std::cout << "[PASSED] Concurrency and WAL Tests.\n";

        std::cout << "\n[RUNNING] Query Execution Tests...\n";
        run_query_execution_tests();
        std::cout << "[PASSED] Query Execution Tests.\n";

        std::cout << "\n[RUNNING] Advanced Query Operator Tests...\n";
        run_advanced_query_tests();
        std::cout << "[PASSED] Advanced Query Operator Tests.\n";

        std::cout << "\n[RUNNING] Validation and Projection Tests...\n";
        run_validation_and_projection_tests();
        std::cout << "[PASSED] Validation and Projection Tests.\n";

        std::cout << "\n[RUNNING] Storage and Partitioning Tests...\n";
        run_storage_and_partitioning_tests();
        std::cout << "[PASSED] Storage and Partitioning Tests.\n";

        std::cout << "\n[RUNNING] Disk Storage Stress Tests...\n";
        run_disk_storage_stress_tests();
        std::cout << "[PASSED] Disk Storage Stress Tests.\n";

        std::cout << "\n[RUNNING] Coordinator and Backup Tests...\n";
        run_coordinator_and_backup_tests();
        std::cout << "[PASSED] Coordinator and Backup Tests.\n";

        std::cout << "\n[RUNNING] Distributed 2PC Tests...\n";
        run_distributed_tests();
        std::cout << "[PASSED] Distributed 2PC Tests.\n";

        std::cout << "\n[RUNNING] Advanced Performance Benchmarks...\n";
        run_advanced_performance_benchmarks();
        std::cout << "[PASSED] Advanced Performance Benchmarks.\n";

        std::cout << "\n[RUNNING] Performance Stress Query Benchmarks...\n";
        run_performance_stress_queries_tests();
        std::cout << "[PASSED] Performance Stress Query Benchmarks.\n";

        std::cout << "\n[RUNNING] Performance Benchmarks...\n";
        run_performance_benchmarks();
        std::cout << "[PASSED] Performance Benchmarks.\n";

        std::cout << "\n[RUNNING] Transaction Stress Tests...\n";
        run_transaction_stress_tests();
        std::cout << "[PASSED] Transaction Stress Tests.\n";

        std::cout << "\n==================================================\n";
        std::cout << "            ALL TESTS PASSED SUCCESSFULLY!        \n";
        std::cout << "==================================================\n";
    } catch (const std::exception& ex) {
        std::cerr << "\n[FAILED] Test suite failed with exception: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n[FAILED] Test suite failed with unknown exception.\n";
        return 1;
    }

    return 0;
}
