#ifndef AETHER_GRAPH_GRAPH_ENGINE_H
#define AETHER_GRAPH_GRAPH_ENGINE_H

#include "utils.h"
#include "storage.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace AetherGraph {

enum class DataType : uint8_t {
    NIL = 0,
    INT = 1,
    FLOAT = 2,
    STRING = 3,
    VECTOR = 4
};

class PropertyValue {
public:
    virtual ~PropertyValue() = default;
    virtual DataType get_type() const = 0;
};

class IntValue : public PropertyValue {
public:
    int32_t val;
    explicit IntValue(int32_t v) : val(v) {}
    DataType get_type() const override { return DataType::INT; }
};

class FloatValue : public PropertyValue {
public:
    float val;
    explicit FloatValue(float v) : val(v) {}
    DataType get_type() const override { return DataType::FLOAT; }
};

class StringValue : public PropertyValue {
public:
    std::string val;
    explicit StringValue(const std::string& v) : val(v) {}
    DataType get_type() const override { return DataType::STRING; }
};

class VectorValue : public PropertyValue {
public:
    std::vector<float> val;
    explicit VectorValue(const std::vector<float>& v) : val(v) {}
    DataType get_type() const override { return DataType::VECTOR; }
};

struct Variant {
    DataType type;
    PropertyValue* val_ptr = nullptr;

    Variant();
    explicit Variant(int32_t val);
    explicit Variant(float val);
    explicit Variant(const std::string& val);
    explicit Variant(const std::vector<float>& val);
    ~Variant();

    Variant(const Variant& other);
    Variant& operator=(const Variant& other);
    Variant(Variant&& other) noexcept;
    Variant& operator=(Variant&& other) noexcept;

    int32_t get_int() const;
    float get_float() const;
    std::string get_string() const;
    std::vector<float> get_vector() const;

    void clear();
    bool operator==(const Variant& other) const;
    bool operator!=(const Variant& other) const { return !(*this == other); }
};

// MVCC version chain for property versioning
struct PropertyVersion {
    Variant value;
    txn_id_t write_txn;
    PropertyVersion* prev;

    PropertyVersion(const Variant& v, txn_id_t txn, PropertyVersion* p)
        : value(v), write_txn(txn), prev(p) {}
};

struct Edge {
    edge_id_t id;
    node_id_t src_id;
    node_id_t dest_id;
    std::string type;
    std::unordered_map<std::string, Variant> properties;

    Edge(edge_id_t eid, node_id_t src, node_id_t dest, const std::string& etype)
        : id(eid), src_id(src), dest_id(dest), type(etype) {}
};

struct Node {
    node_id_t id;
    std::string label;
    std::unordered_map<std::string, Variant> properties;
    std::unordered_map<std::string, PropertyVersion*> version_heads;

    // Outgoing edges
    Edge** out_edges = nullptr;
    size_t out_edges_count = 0;
    size_t out_edges_capacity = 0;

    // Incoming edge references for cleanup
    std::vector<edge_id_t> in_edge_ids;

    Node(node_id_t nid, const std::string& nlabel)
        : id(nid), label(nlabel) {}

    ~Node();
    void add_out_edge(Edge* edge);
    void remove_out_edge(edge_id_t edge_id);
    void free_version_chains();
};

class GraphEngine {
private:
    BufferPoolManager& cache_mgr_;
    std::unordered_map<node_id_t, Node*> nodes_;
    std::unordered_map<edge_id_t, Edge*> edges_;
    node_id_t next_node_id_ = 1;
    edge_id_t next_edge_id_ = 1;

    // Shared traversal state for DFS/BFS queries
    std::unordered_set<node_id_t> traversal_visited_;
    size_t traversal_match_count_ = 0;

    void trim_old_versions(Node* node, const std::string& key);
    void rebuild_adjacency_index();

public:
    explicit GraphEngine(BufferPoolManager& cache_mgr);
    ~GraphEngine();

    Node* create_node(const std::string& label);
    Edge* create_edge(node_id_t src_id, node_id_t dest_id, const std::string& type);

    bool delete_node(node_id_t node_id);
    Node* get_node(node_id_t node_id);
    Edge* get_edge(edge_id_t edge_id);

    const std::unordered_map<node_id_t, Node*>& get_all_nodes() const { return nodes_; }
    const std::unordered_map<edge_id_t, Edge*>& get_all_edges() const { return edges_; }

    // Property versioning (MVCC)
    void update_property(node_id_t node_id, const std::string& key,
                         const Variant& value, txn_id_t txn_id);
    PropertyVersion* resolve_property(node_id_t node_id, const std::string& key,
                                       txn_id_t read_ts);

    // Traversal state management
    void mark_visited(node_id_t nid) { traversal_visited_.insert(nid); }
    bool is_visited(node_id_t nid) const { return traversal_visited_.count(nid) > 0; }
    void reset_traversal_state();
    void record_traversal_match(node_id_t nid);

    // Serialization helpers
    Variant deserialize_variant(const uint8_t* data, size_t size, size_t& offset);

    void clear();
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_ENGINE_H
