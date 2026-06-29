#ifndef AETHER_GRAPH_EXECUTION_PLANS_H
#define AETHER_GRAPH_EXECUTION_PLANS_H

#include "graph_engine.h"
#include "property_index.h"
#include <vector>
#include <memory>

namespace AetherGraph {

class PhysicalOperator {
public:
    virtual ~PhysicalOperator() = default;
    virtual void open() = 0;
    virtual Node* next() = 0;
    virtual void close() = 0;
};

class PhysicalSeqScan : public PhysicalOperator {
private:
    GraphEngine& ge_;
    std::string label_;
    std::vector<node_id_t> nodes_;
    size_t cursor_ = 0;

public:
    PhysicalSeqScan(GraphEngine& ge, const std::string& label);
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalIndexScan : public PhysicalOperator {
private:
    GraphEngine& ge_;
    PropertyIndex& index_;
    std::string key_val_;
    std::vector<node_id_t> matching_ids_;
    size_t cursor_ = 0;

public:
    PhysicalIndexScan(GraphEngine& ge, PropertyIndex& index, const std::string& key_val);
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalFilter : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> child_;
    std::string key_;
    std::string op_;
    Variant val_;

    bool evaluate(const Node* node) const;

public:
    PhysicalFilter(std::unique_ptr<PhysicalOperator> child, const std::string& key, const std::string& op, const Variant& val);
    void open() override;
    Node* next() override;
    void close() override;
};

class PhysicalNestedLoopJoin : public PhysicalOperator {
private:
    std::unique_ptr<PhysicalOperator> left_;
    std::unique_ptr<PhysicalOperator> right_;
    
    Node* left_node_ = nullptr;
    std::vector<Node*> right_matches_;
    size_t right_cursor_ = 0;

public:
    PhysicalNestedLoopJoin(std::unique_ptr<PhysicalOperator> left, std::unique_ptr<PhysicalOperator> right);
    void open() override;
    Node* next() override;
    void close() override;
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_EXECUTION_PLANS_H
