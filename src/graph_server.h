#ifndef AETHER_GRAPH_GRAPH_SERVER_H
#define AETHER_GRAPH_GRAPH_SERVER_H

#include "graph_engine.h"
#include "transaction_manager.h"
#include "concurrency_control.h"
#include "cypher_parser.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace AetherGraph {

struct ClientSession {
    uint32_t session_id;
    Transaction* active_txn = nullptr;
    std::vector<std::string> query_history;
};

class GraphServer {
private:
    GraphEngine& ge_;
    TransactionManager& tm_;
    ConcurrencyControl& cc_;
    
    std::unordered_map<uint32_t, ClientSession> sessions_;
    uint32_t next_session_id_ = 1000;
    std::mutex mutex_;

public:
    GraphServer(GraphEngine& ge, TransactionManager& tm, ConcurrencyControl& cc);
    ~GraphServer();

    uint32_t connect_client();
    void disconnect_client(uint32_t session_id);
    
    std::string execute_query(uint32_t session_id, const std::string& cypher_query);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_SERVER_H
