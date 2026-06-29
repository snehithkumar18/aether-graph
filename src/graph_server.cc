#include "graph_server.h"
#include "graph_serializer.h"
#include <sstream>

namespace AetherGraph {

GraphServer::GraphServer(GraphEngine& ge, TransactionManager& tm, ConcurrencyControl& cc)
    : ge_(ge), tm_(tm), cc_(cc) {}

GraphServer::~GraphServer() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [sid, session] : sessions_) {
        if (session.active_txn) {
            tm_.abort(session.active_txn, ge_);
            cc_.release_all_locks(session.active_txn->get_txn_id());
        }
    }
}

uint32_t GraphServer::connect_client() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t sid = next_session_id_++;
    ClientSession session;
    session.session_id = sid;
    sessions_[sid] = session;
    return sid;
}

void GraphServer::disconnect_client(uint32_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        if (it->second.active_txn) {
            tm_.abort(it->second.active_txn, ge_);
            cc_.release_all_locks(it->second.active_txn->get_txn_id());
        }
        sessions_.erase(it);
    }
}

std::string GraphServer::execute_query(uint32_t session_id, const std::string& cypher_query) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto s_it = sessions_.find(session_id);
    if (s_it == sessions_.end()) {
        return "{\"status\": \"error\", \"message\": \"Invalid session ID\"}";
    }

    auto& session = s_it->second;
    session.query_history.push_back(cypher_query);

    // Auto-begin transaction if none active
    if (!session.active_txn) {
        session.active_txn = tm_.begin_transaction();
    }

    Transaction* txn = session.active_txn;
    txn_id_t tid = txn->get_txn_id();

    // Parse the query
    ASTStatement ast = CypherParser::parse(cypher_query);

    if (ast.op == "MATCH") {
        std::vector<Node*> results;
        
        // Acquire shared lock on matched patterns
        for (const auto& pattern : ast.patterns) {
            if (!pattern.start.variable.empty()) {
                // Lock resources matching labels
                const auto& nodes = ge_.get_all_nodes();
                for (const auto& [nid, n] : nodes) {
                    if (n->label == pattern.start.label) {
                        cc_.acquire_lock(tid, n->id, LockMode::SHARED);
                    }
                }
            }
        }

        // Apply filters
        for (const auto& pred : ast.predicates) {
            const auto& nodes = ge_.get_all_nodes();
            for (const auto& [nid, n] : nodes) {
                auto prop_it = n->properties.find(pred.property_key);
                if (prop_it != n->properties.end() && prop_it->second == pred.val) {
                    results.push_back(n);
                }
            }
        }

        // Commit transaction for matches (auto commit read-only)
        tm_.commit(txn);
        cc_.release_all_locks(tid);
        session.active_txn = nullptr;

        // Serialize results to JSON
        std::ostringstream oss;
        oss << "{\"status\": \"success\", \"results\": [";
        for (size_t r_idx = 0; r_idx < results.size(); ++r_idx) {
            oss << "{\"id\": " << results[r_idx]->id << ", \"label\": \"" << results[r_idx]->label << "\"}";
            if (r_idx + 1 < results.size()) oss << ",";
        }
        oss << "]}";
        return oss.str();
    } 
    
    if (ast.op == "CREATE") {
        for (const auto& pattern : ast.patterns) {
            // Lock new entity ID range
            cc_.acquire_lock(tid, tid + 2000000, LockMode::EXCLUSIVE);
            Node* n = ge_.create_node(pattern.start.label);
            txn->append_undo(UndoRecord(n->id, "", Variant(), true));
            cc_.release_lock(tid, tid + 2000000);
        }

        // Keep transaction active for writes until explicit commit
        std::ostringstream oss;
        oss << "{\"status\": \"success\", \"message\": \"Created entities, transaction active\"}";
        return oss.str();
    }

    return "{\"status\": \"error\", \"message\": \"Unsupported query type\"}";
}

} // namespace AetherGraph
