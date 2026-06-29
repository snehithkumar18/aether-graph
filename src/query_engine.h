#ifndef AETHER_GRAPH_QUERY_ENGINE_H
#define AETHER_GRAPH_QUERY_ENGINE_H

#include "execution_plans.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace AetherGraph {

class PhysicalHashJoin : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> left_;
    std::unique_ptr<PhysicalOperator> right_;
    std::string left_key_;
    std::string right_key_;
    
    std::unordered_multimap<std::string, Node*> hash_table_;
    std::vector<Node*> matches_;
    size_t cursor_ = 0;

public:
    PhysicalHashJoin(std::unique_ptr<PhysicalOperator> left, std::unique_ptr<PhysicalOperator> right, const std::string& left_key, const std::string& right_key);
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalSort : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> child_;
    std::string sort_key_;
    bool ascending_;
    std::vector<Node*> sorted_nodes_;
    size_t cursor_ = 0;

public:
    PhysicalSort(std::unique_ptr<PhysicalOperator> child, const std::string& sort_key, bool ascending = true);
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalLimit : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> child_;
    size_t limit_;
    size_t count_ = 0;

public:
    PhysicalLimit(std::unique_ptr<PhysicalOperator> child, size_t limit);
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalAggregate : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> child_;
    std::string group_key_;
    std::string agg_key_;
    std::string func_; // "COUNT", "SUM", "MIN", "MAX", "AVG"

    std::vector<Node*> result_nodes_;
    size_t cursor_ = 0;

    void compute_aggregates();

public:
    PhysicalAggregate(std::unique_ptr<PhysicalOperator> child, const std::string& group_key, const std::string& agg_key, const std::string& func);
    ~PhysicalAggregate();
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalUnique : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> child_;
    std::unordered_set<node_id_t> seen_ids_;

public:
    PhysicalUnique(std::unique_ptr<PhysicalOperator> child);
    void open() override;
    Node* next() override;
    void close() override;
};

class QueryEngineCoordinator {
private:
    GraphEngine& ge_;

public:
    explicit QueryEngineCoordinator(GraphEngine& ge) : ge_(ge) {}
    ~QueryEngineCoordinator() = default;

    std::vector<Node*> execute_plan(std::unique_ptr<PhysicalOperator> plan);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_ENGINE_H
