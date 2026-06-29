#ifndef AETHER_GRAPH_PROPERTY_INDEX_H
#define AETHER_GRAPH_PROPERTY_INDEX_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <memory>

namespace AetherGraph {

struct IndexBTreeNode {
    bool is_leaf;
    std::vector<std::string> keys;
    std::vector<std::vector<node_id_t>> values; // node IDs that match this key
    std::vector<std::unique_ptr<IndexBTreeNode>> children;

    explicit IndexBTreeNode(bool leaf) : is_leaf(leaf) {}
};

class PropertyIndex {
private:
    std::unique_ptr<IndexBTreeNode> root_;
    size_t min_degree_; // B-Tree degree parameter
    std::string property_key_;

    void insert_non_full(IndexBTreeNode* node, const std::string& key, node_id_t value);
    void split_child(IndexBTreeNode* node, size_t i, IndexBTreeNode* child);
    void search_recursive(IndexBTreeNode* node, const std::string& key, std::vector<node_id_t>& results);
    void range_search_recursive(IndexBTreeNode* node, const std::string& min_key, const std::string& max_key, std::vector<node_id_t>& results);
    void remove_recursive(IndexBTreeNode* node, const std::string& key, node_id_t value);

public:
    PropertyIndex(const std::string& property_key, size_t min_degree = 4);
    ~PropertyIndex() = default;

    void insert(const std::string& key, node_id_t value);
    void remove(const std::string& key, node_id_t value);
    std::vector<node_id_t> search(const std::string& key);
    std::vector<node_id_t> range_search(const std::string& min_key, const std::string& max_key);
    void build_from_graph(GraphEngine& ge);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_PROPERTY_INDEX_H
