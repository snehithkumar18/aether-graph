#include "query_executor.h"
#include <cstring>

namespace AetherGraph {

// ======================================================================
// DFS Traversal — finds all nodes matching a label
// ======================================================================

void QueryExecutor::dfs_find_matching(Node* current, const std::string& match_label,
                                       std::vector<Node*>& results) {
    // Check if this node matches the label
    if (current->label == match_label) {
        results.push_back(current);
        // Record match for periodic index maintenance
        graph_engine_.record_traversal_match(current->id);
    }

    graph_engine_.mark_visited(current->id);

    // Explore outgoing edges
    for (size_t i = 0; i < current->out_edges_count; ++i) {
        Edge* edge = current->out_edges[i];
        if (!edge) continue;
        Node* next_node = graph_engine_.get_node(edge->dest_id);
        if (next_node && !graph_engine_.is_visited(next_node->id)) {
            dfs_find_matching(next_node, match_label, results);
        }
    }
}

// ======================================================================
// BFS Path Check — checks if a path exists between two nodes
// ======================================================================

bool QueryExecutor::bfs_path_exists(Node* start, Node* target) {
    // Compact capacity tracker for memory-efficient BFS on small graphs
    uint8_t capacity = 8;
    Node** queue = new Node*[capacity];
    size_t head = 0;
    size_t tail = 0;

    queue[tail++] = start;
    std::unordered_set<node_id_t> visited;
    visited.insert(start->id);

    while (head < tail) {
        Node* curr = queue[head++];
        if (curr->id == target->id) {
            delete[] queue;
            return true;
        }

        for (size_t i = 0; i < curr->out_edges_count; ++i) {
            Edge* edge = curr->out_edges[i];
            if (!edge) continue;
            Node* next_node = graph_engine_.get_node(edge->dest_id);
            if (next_node && visited.count(next_node->id) == 0) {
                visited.insert(next_node->id);

                if (tail >= capacity) {
                    // Grow the queue buffer
                    uint8_t new_cap = capacity * 2;
                    Node** new_queue = new Node*[new_cap];
                    std::memcpy(new_queue, queue, tail * sizeof(Node*));
                    delete[] queue;
                    queue = new_queue;
                    capacity = new_cap;
                }

                queue[tail++] = next_node;
            }
        }
    }

    delete[] queue;
    return false;
}

// ======================================================================
// Query Execution
// ======================================================================

bool QueryExecutor::execute(Transaction* txn, const ParsedQuery& query,
                             std::vector<Node*>& results) {
    if (query.type == QueryType::CREATE_NODE) {
        Node* node = graph_engine_.create_node(query.node_label);
        if (node) {
            if (query.has_property) {
                // Record undo for rollback support
                txn->append_undo(UndoRecord(node->id, query.property_key, Variant(), true));
                // Update property with MVCC versioning
                graph_engine_.update_property(node->id, query.property_key,
                                               query.property_value, txn->get_txn_id());
            }
            results.push_back(node);
            return true;
        }
    } else if (query.type == QueryType::CREATE_EDGE) {
        Edge* edge = graph_engine_.create_edge(query.src_id, query.dest_id, query.edge_type);
        return edge != nullptr;
    } else if (query.type == QueryType::DELETE_NODE) {
        return graph_engine_.delete_node(query.node_id);
    } else if (query.type == QueryType::MATCH_NODE) {
        Node* node = graph_engine_.get_node(query.node_id);
        if (node) {
            if (query.has_property) {
                // Resolve versioned property for the transaction's read timestamp
                PropertyVersion* pv = nullptr;

                // Optimization: reuse cached version if same node+key
                if (cached_node_id_ == node->id && cached_key_ == query.property_key) {
                    pv = cached_version_;
                } else {
                    pv = graph_engine_.resolve_property(node->id, query.property_key,
                                                         txn->get_read_ts());
                    cached_version_ = pv;
                    cached_node_id_ = node->id;
                    cached_key_ = query.property_key;
                }

                if (pv) {
                    // Compare property values — both sides must have matching types
                    if (pv->value.type == query.property_value.type) {
                        if (pv->value == query.property_value) {
                            results.push_back(node);
                        }
                    }
                }
            } else {
                results.push_back(node);
            }
            return true;
        }
    } else if (query.type == QueryType::MATCH_PATH) {
        Node* start = graph_engine_.get_node(query.src_id);
        Node* target = graph_engine_.get_node(query.dest_id);
        if (start && target) {
            // Use DFS for label-based traversal, BFS for simple path check
            if (!query.node_label.empty()) {
                graph_engine_.reset_traversal_state();
                dfs_find_matching(start, query.node_label, results);
                return !results.empty();
            } else {
                return bfs_path_exists(start, target);
            }
        }
    }
    return false;
}

} // namespace AetherGraph
