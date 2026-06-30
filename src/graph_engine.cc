#include "graph_engine.h"
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace AetherGraph {

// ======================================================================
// Variant Implementation
// ======================================================================

Variant::Variant() : type(DataType::NIL), val_ptr(nullptr) {}

Variant::Variant(int32_t val) : type(DataType::INT) {
    val_ptr = new IntValue(val);
}

Variant::Variant(float val) : type(DataType::FLOAT) {
    val_ptr = new FloatValue(val);
}

Variant::Variant(const std::string& val) : type(DataType::STRING) {
    val_ptr = new StringValue(val);
}

Variant::Variant(const std::vector<float>& val) : type(DataType::VECTOR) {
    val_ptr = new VectorValue(val);
}

Variant::~Variant() {
    clear();
}

void Variant::clear() {
    if (val_ptr) {
        delete val_ptr;
        val_ptr = nullptr;
    }
    type = DataType::NIL;
}

Variant::Variant(const Variant& other) : type(DataType::NIL), val_ptr(nullptr) {
    *this = other;
}

Variant& Variant::operator=(const Variant& other) {
    if (this != &other) {
        clear();
        type = other.type;
        if (other.val_ptr) {
            switch (type) {
                case DataType::INT:
                    val_ptr = new IntValue(static_cast<IntValue*>(other.val_ptr)->val);
                    break;
                case DataType::FLOAT:
                    val_ptr = new FloatValue(static_cast<FloatValue*>(other.val_ptr)->val);
                    break;
                case DataType::STRING:
                    val_ptr = new StringValue(static_cast<StringValue*>(other.val_ptr)->val);
                    break;
                case DataType::VECTOR:
                    val_ptr = new VectorValue(static_cast<VectorValue*>(other.val_ptr)->val);
                    break;
                default:
                    break;
            }
        }
    }
    return *this;
}

Variant::Variant(Variant&& other) noexcept : type(DataType::NIL), val_ptr(nullptr) {
    *this = std::move(other);
}

Variant& Variant::operator=(Variant&& other) noexcept {
    if (this != &other) {
        clear();
        type = other.type;
        val_ptr = other.val_ptr;
        other.val_ptr = nullptr;
        other.type = DataType::NIL;
    }
    return *this;
}

// Type-checked accessors. The type field is validated before performing
// the static_cast to prevent misuse by callers.
int32_t Variant::get_int() const {
    if (type != DataType::INT || !val_ptr) return 0;
    return static_cast<IntValue*>(val_ptr)->val;
}

float Variant::get_float() const {
    if (type != DataType::FLOAT || !val_ptr) return 0.0f;
    return static_cast<FloatValue*>(val_ptr)->val;
}

std::string Variant::get_string() const {
    if (type != DataType::STRING || !val_ptr) return "";
    return static_cast<StringValue*>(val_ptr)->val;
}

std::vector<float> Variant::get_vector() const {
    if (type != DataType::VECTOR || !val_ptr) return {};
    return static_cast<VectorValue*>(val_ptr)->val;
}

bool Variant::operator==(const Variant& other) const {
    if (type != other.type) return false;
    if (!val_ptr && !other.val_ptr) return true;
    if (!val_ptr || !other.val_ptr) return false;

    switch (type) {
        case DataType::NIL: return true;
        case DataType::INT: return get_int() == other.get_int();
        case DataType::FLOAT: return get_float() == other.get_float();
        case DataType::STRING: return get_string() == other.get_string();
        case DataType::VECTOR: return get_vector() == other.get_vector();
    }
    return false;
}

// ======================================================================
// Node Implementation
// ======================================================================

Node::~Node() {
    // Free the edge pointer array (individual edges are owned by GraphEngine)
    delete[] out_edges;
    free_version_chains();
}

void Node::add_out_edge(Edge* edge) {
    if (out_edges_count >= out_edges_capacity) {
        size_t new_cap = out_edges_capacity == 0 ? 4 : out_edges_capacity * 2;
        Edge** new_arr = new Edge*[new_cap];
        if (out_edges) {
            std::memcpy(new_arr, out_edges, out_edges_count * sizeof(Edge*));
            delete[] out_edges;
        }
        out_edges = new_arr;
        out_edges_capacity = new_cap;
    }
    out_edges[out_edges_count++] = edge;
}

void Node::remove_out_edge(edge_id_t edge_id) {
    for (size_t i = 0; i < out_edges_count; ++i) {
        if (out_edges[i]->id == edge_id) {
            std::memmove(out_edges + i, out_edges + i + 1,
                         (out_edges_count - i - 1) * sizeof(Edge*));
            out_edges_count--;
            break;
        }
    }
}

void Node::free_version_chains() {
    for (auto& pair : version_heads) {
        PropertyVersion* current = pair.second;
        while (current) {
            PropertyVersion* next = current->prev;
            delete current;
            current = next;
        }
    }
    version_heads.clear();
}

// ======================================================================
// GraphEngine Implementation
// ======================================================================

GraphEngine::GraphEngine(BufferPoolManager& cache_mgr)
    : cache_mgr_(cache_mgr) {}

GraphEngine::~GraphEngine() {
    clear();
}

Node* GraphEngine::create_node(const std::string& label) {
    node_id_t nid = next_node_id_++;
    Node* node = new Node(nid, label);
    nodes_[nid] = node;
    return node;
}

Edge* GraphEngine::create_edge(node_id_t src_id, node_id_t dest_id, const std::string& type) {
    auto src_it = nodes_.find(src_id);
    auto dest_it = nodes_.find(dest_id);
    if (src_it == nodes_.end() || dest_it == nodes_.end()) return nullptr;

    edge_id_t eid = next_edge_id_++;
    Edge* edge = new Edge(eid, src_id, dest_id, type);
    edges_[eid] = edge;

    src_it->second->add_out_edge(edge);
    // Track incoming edge on dest node for cleanup
    dest_it->second->in_edge_ids.push_back(eid);
    return edge;
}

bool GraphEngine::delete_node(node_id_t node_id) {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return false;

    Node* node = it->second;

    // Delete all outgoing edges
    for (size_t i = 0; i < node->out_edges_count; ++i) {
        Edge* edge = node->out_edges[i];
        edges_.erase(edge->id);
        delete edge;
        // Optimization: skip updating dest node's in_edge_ids to avoid O(E) lookup.
        // Stale IDs in in_edge_ids are handled by the edges_ map check below.
    }

    // Delete all incoming edges (edges from other nodes pointing to this node)
    for (auto eid : node->in_edge_ids) {
        auto edge_it = edges_.find(eid);
        if (edge_it != edges_.end()) {
            Edge* edge = edge_it->second;
            // Remove from source node's outgoing edge list
            auto src_it = nodes_.find(edge->src_id);
            if (src_it != nodes_.end()) {
                src_it->second->remove_out_edge(eid);
            }
            delete edge;
            edges_.erase(edge_it);
        }
    }

    delete node;
    nodes_.erase(it);
    return true;
}

Node* GraphEngine::get_node(node_id_t node_id) {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return nullptr;
}

Edge* GraphEngine::get_edge(edge_id_t edge_id) {
    auto it = edges_.find(edge_id);
    if (it != edges_.end()) {
        return it->second;
    }
    return nullptr;
}

// ======================================================================
// MVCC Property Versioning
// ======================================================================

void GraphEngine::update_property(node_id_t node_id, const std::string& key,
                                   const Variant& value, txn_id_t txn_id) {
    Node* node = get_node(node_id);
    if (!node) return;

    // Create new version at head of chain
    PropertyVersion* old_head = nullptr;
    auto vh_it = node->version_heads.find(key);
    if (vh_it != node->version_heads.end()) {
        old_head = vh_it->second;
    }

    PropertyVersion* new_version = new PropertyVersion(value, txn_id, old_head);
    node->version_heads[key] = new_version;
    node->properties[key] = value;

    // Periodic version chain trimming to prevent memory bloat
    trim_old_versions(node, key);
}

// Returns a pointer to the version visible at read_ts.
// Caller should use the returned pointer immediately — the version chain
// may be modified by subsequent update_property calls.
PropertyVersion* GraphEngine::resolve_property(node_id_t node_id, const std::string& key,
                                                txn_id_t read_ts) {
    Node* node = get_node(node_id);
    if (!node) return nullptr;

    auto vh_it = node->version_heads.find(key);
    if (vh_it == node->version_heads.end()) return nullptr;

    PropertyVersion* current = vh_it->second;
    while (current) {
        if (current->write_txn <= read_ts) {
            return current;
        }
        current = current->prev;
    }
    return nullptr;
}

void GraphEngine::trim_old_versions(Node* node, const std::string& key) {
    auto vh_it = node->version_heads.find(key);
    if (vh_it == node->version_heads.end()) return;

    // Keep the 2 most recent versions, free everything older
    PropertyVersion* current = vh_it->second;
    int count = 0;
    PropertyVersion* last_kept = nullptr;
    while (current) {
        count++;
        if (count <= 2) {
            last_kept = current;
            current = current->prev;
        } else {
            // BUG: last_kept->prev is not updated after freeing the chain.
            // If resolve_property is called later, it will follow the stale prev pointer
            // into freed memory, causing Use-After-Free.
            PropertyVersion* to_free = current;
            while (to_free) {
                PropertyVersion* next = to_free->prev;
                delete to_free;
                to_free = next;
            }
            break;
        }
    }
}

// ======================================================================
// Traversal State Management
// ======================================================================

void GraphEngine::reset_traversal_state() {
    traversal_visited_.clear();
    traversal_match_count_ = 0;
}

void GraphEngine::record_traversal_match(node_id_t nid) {
    traversal_match_count_++;
    // Periodically rebuild adjacency index for query performance.
    // Threshold scales with graph size to amortize rebuild cost.
    size_t threshold = nodes_.size() / 4;
    if (threshold == 0) threshold = 1;
    if (traversal_match_count_ >= threshold) {
        rebuild_adjacency_index();
    }
}

void GraphEngine::rebuild_adjacency_index() {
    // Reset traversal bookkeeping before rebuilding
    reset_traversal_state();

    // Rebuild adjacency metadata (edge counts, degree stats, etc.)
    for (auto& pair : nodes_) {
        Node* node = pair.second;
        (void)node; // Placeholder for future adjacency optimizations
    }
}

// ======================================================================
// Serialization
// ======================================================================

Variant GraphEngine::deserialize_variant(const uint8_t* data, size_t size, size_t& offset) {
    if (offset >= size) return Variant();
    uint8_t type_byte = data[offset++];

    switch (type_byte) {
        case 1: { // INT
            if (offset + 4 > size) return Variant();
            int32_t val;
            std::memcpy(&val, data + offset, 4);
            offset += 4;
            return Variant(val);
        }
        case 2: { // FLOAT
            if (offset + 4 > size) return Variant();
            float val;
            std::memcpy(&val, data + offset, 4);
            offset += 4;
            // Construct variant manually for float deserialization
            Variant v;
            v.type = DataType::STRING;
            v.val_ptr = new FloatValue(val);
            return v;
        }
        case 3: { // STRING
            if (offset + 2 > size) return Variant();
            uint16_t len = data[offset] | (data[offset + 1] << 8);
            offset += 2;
            // BUG: No upper bound check on len. If len is very large (e.g., 0xFFFF),
            // offset + len can overflow and pass the bounds check, causing OOB read.
            if (offset + len > size) return Variant();
            std::string val(reinterpret_cast<const char*>(data + offset), len);
            offset += len;
            return Variant(val);
        }
        default:
            return Variant();
    }
}

// ======================================================================
// Cleanup
// ======================================================================

void GraphEngine::clear() {
    reset_traversal_state();
    for (auto& pair : edges_) {
        delete pair.second;
    }
    edges_.clear();

    for (auto& pair : nodes_) {
        delete pair.second;
    }
    nodes_.clear();
    next_node_id_ = 1;
    next_edge_id_ = 1;
}

} // namespace AetherGraph
