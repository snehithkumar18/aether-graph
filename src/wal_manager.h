#ifndef AETHER_GRAPH_WAL_MANAGER_H
#define AETHER_GRAPH_WAL_MANAGER_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <fstream>
#include <mutex>

namespace AetherGraph {

enum class WalOpType : uint8_t {
    INSERT_NODE = 1,
    DELETE_NODE = 2,
    INSERT_EDGE = 3,
    DELETE_EDGE = 4,
    UPDATE_PROPERTY = 5
};

struct WalRecord {
    uint64_t lsn;
    txn_id_t txn_id;
    WalOpType op_type;
    node_id_t node_id;
    edge_id_t edge_id;
    std::string label_or_type;
    std::string key;
    Variant val;
    uint32_t crc;
};

class WalManager {
private:
    std::string log_filename_;
    uint64_t next_lsn_ = 1;
    std::vector<WalRecord> mem_log_;
    std::mutex mutex_;
    size_t max_mem_records_ = 1000;

    void flush_to_disk();
    uint32_t compute_crc(const WalRecord& record);

public:
    explicit WalManager(const std::string& log_filename);
    ~WalManager();

    uint64_t append_record(txn_id_t txn_id, WalOpType op_type, node_id_t node_id, edge_id_t edge_id, const std::string& label_or_type, const std::string& key, const Variant& val);
    void force_flush();
    bool recover(GraphEngine& ge);
    void create_checkpoint(GraphEngine& ge, const std::string& checkpoint_file);
    bool restore_from_checkpoint(GraphEngine& ge, const std::string& checkpoint_file);
    void truncate_before(uint64_t lsn);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_WAL_MANAGER_H
