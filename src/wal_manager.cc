#include "wal_manager.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace AetherGraph {

WalManager::WalManager(const std::string& log_filename) : log_filename_(log_filename) {
    // Open log file in append mode or create if missing
    std::ofstream out(log_filename_, std::ios::binary | std::ios::app);
}

WalManager::~WalManager() {
    force_flush();
}

uint32_t WalManager::compute_crc(const WalRecord& record) {
    uint32_t crc = 0xFFFFFFFF;
    auto crc_update = [&](const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
                else crc >>= 1;
            }
        }
    };

    crc_update(reinterpret_cast<const uint8_t*>(&record.lsn), 8);
    crc_update(reinterpret_cast<const uint8_t*>(&record.txn_id), 4);
    uint8_t op = static_cast<uint8_t>(record.op_type);
    crc_update(&op, 1);
    crc_update(reinterpret_cast<const uint8_t*>(&record.node_id), 4);
    crc_update(reinterpret_cast<const uint8_t*>(&record.edge_id), 4);
    crc_update(reinterpret_cast<const uint8_t*>(record.label_or_type.data()), record.label_or_type.size());
    crc_update(reinterpret_cast<const uint8_t*>(record.key.data()), record.key.size());
    
    uint8_t vt = static_cast<uint8_t>(record.val.type);
    crc_update(&vt, 1);
    if (record.val.type == DataType::INT) {
        int32_t val = record.val.get_int();
        crc_update(reinterpret_cast<const uint8_t*>(&val), 4);
    } else if (record.val.type == DataType::FLOAT) {
        float val = record.val.get_float();
        crc_update(reinterpret_cast<const uint8_t*>(&val), 4);
    } else if (record.val.type == DataType::STRING) {
        std::string s = record.val.get_string();
        crc_update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    return ~crc;
}

uint64_t WalManager::append_record(
    txn_id_t txn_id, WalOpType op_type, node_id_t node_id, edge_id_t edge_id,
    const std::string& label_or_type, const std::string& key, const Variant& val) {

    std::lock_guard<std::mutex> lock(mutex_);
    WalRecord rec;
    rec.lsn = next_lsn_++;
    rec.txn_id = txn_id;
    rec.op_type = op_type;
    rec.node_id = node_id;
    rec.edge_id = edge_id;
    rec.label_or_type = label_or_type;
    rec.key = key;
    rec.val = val;
    rec.crc = compute_crc(rec);

    mem_log_.push_back(rec);
    if (mem_log_.size() >= max_mem_records_) {
        flush_to_disk();
    }
    return rec.lsn;
}

void WalManager::flush_to_disk() {
    if (mem_log_.empty()) return;
    std::ofstream out(log_filename_, std::ios::binary | std::ios::app);
    if (!out) return;

    for (const auto& rec : mem_log_) {
        out.write(reinterpret_cast<const char*>(&rec.lsn), 8);
        out.write(reinterpret_cast<const char*>(&rec.txn_id), 4);
        uint8_t op = static_cast<uint8_t>(rec.op_type);
        out.write(reinterpret_cast<const char*>(&op), 1);
        out.write(reinterpret_cast<const char*>(&rec.node_id), 4);
        out.write(reinterpret_cast<const char*>(&rec.edge_id), 4);
        
        uint32_t s_len = static_cast<uint32_t>(rec.label_or_type.size());
        out.write(reinterpret_cast<const char*>(&s_len), 4);
        out.write(rec.label_or_type.data(), s_len);

        uint32_t k_len = static_cast<uint32_t>(rec.key.size());
        out.write(reinterpret_cast<const char*>(&k_len), 4);
        out.write(rec.key.data(), k_len);

        uint8_t vt = static_cast<uint8_t>(rec.val.type);
        out.write(reinterpret_cast<const char*>(&vt), 1);
        if (rec.val.type == DataType::INT) {
            int32_t val = rec.val.get_int();
            out.write(reinterpret_cast<const char*>(&val), 4);
        } else if (rec.val.type == DataType::FLOAT) {
            float val = rec.val.get_float();
            out.write(reinterpret_cast<const char*>(&val), 4);
        } else if (rec.val.type == DataType::STRING) {
            std::string s = rec.val.get_string();
            uint32_t v_len = static_cast<uint32_t>(s.size());
            out.write(reinterpret_cast<const char*>(&v_len), 4);
            out.write(s.data(), v_len);
        } else if (rec.val.type == DataType::VECTOR) {
            auto vec = rec.val.get_vector();
            uint32_t v_len = static_cast<uint32_t>(vec.size());
            out.write(reinterpret_cast<const char*>(&v_len), 4);
            for (float val : vec) {
                out.write(reinterpret_cast<const char*>(&val), 4);
            }
        }
        out.write(reinterpret_cast<const char*>(&rec.crc), 4);
    }
    out.flush();
    mem_log_.clear();
}

void WalManager::force_flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    flush_to_disk();
}

bool WalManager::recover(GraphEngine& ge) {
    force_flush();
    std::ifstream in(log_filename_, std::ios::binary);
    if (!in) return false;

    ge.clear();
    std::unordered_map<node_id_t, Node*> node_mapping;

    while (in.peek() != EOF) {
        WalRecord rec;
        in.read(reinterpret_cast<char*>(&rec.lsn), 8);
        in.read(reinterpret_cast<char*>(&rec.txn_id), 4);
        uint8_t op;
        in.read(reinterpret_cast<char*>(&op), 1);
        rec.op_type = static_cast<WalOpType>(op);
        in.read(reinterpret_cast<char*>(&rec.node_id), 4);
        in.read(reinterpret_cast<char*>(&rec.edge_id), 4);

        uint32_t s_len;
        in.read(reinterpret_cast<char*>(&s_len), 4);
        // BUG: No upper bound check on s_len. If s_len is very large (e.g., 0xFFFFFFFF),
        // resize can allocate huge memory or fail, and the subsequent read can cause
        // heap buffer overflow or denial of service.
        rec.label_or_type.resize(s_len);
        in.read(&rec.label_or_type[0], s_len);

        uint32_t k_len;
        in.read(reinterpret_cast<char*>(&k_len), 4);
        rec.key.resize(k_len);
        in.read(&rec.key[0], k_len);

        uint8_t vt;
        in.read(reinterpret_cast<char*>(&vt), 1);
        DataType dt = static_cast<DataType>(vt);
        if (dt == DataType::INT) {
            int32_t val;
            in.read(reinterpret_cast<char*>(&val), 4);
            rec.val = Variant(val);
        } else if (dt == DataType::FLOAT) {
            float val;
            in.read(reinterpret_cast<char*>(&val), 4);
            rec.val = Variant(val);
        } else if (dt == DataType::STRING) {
            uint32_t v_len;
            in.read(reinterpret_cast<char*>(&v_len), 4);
            std::string s;
            s.resize(v_len);
            in.read(&s[0], v_len);
            rec.val = Variant(s);
        } else if (dt == DataType::VECTOR) {
            uint32_t v_len;
            in.read(reinterpret_cast<char*>(&v_len), 4);
            std::vector<float> vec;
            for (uint32_t v_idx = 0; v_idx < v_len; ++v_idx) {
                float val;
                in.read(reinterpret_cast<char*>(&val), 4);
                vec.push_back(val);
            }
            rec.val = Variant(vec);
        }
        in.read(reinterpret_cast<char*>(&rec.crc), 4);

        // Check integrity
        if (rec.crc != compute_crc(rec)) {
            std::cerr << "WAL corrupt at LSN " << rec.lsn << std::endl;
            return false;
        }

        // Apply operations (redo)
        if (rec.op_type == WalOpType::INSERT_NODE) {
            Node* n = ge.create_node(rec.label_or_type);
            node_mapping[rec.node_id] = n;
        } else if (rec.op_type == WalOpType::INSERT_EDGE) {
            if (node_mapping.find(rec.node_id) != node_mapping.end() && node_mapping.find(rec.edge_id) != node_mapping.end()) {
                ge.create_edge(node_mapping[rec.node_id]->id, node_mapping[rec.edge_id]->id, rec.label_or_type);
            }
        } else if (rec.op_type == WalOpType::UPDATE_PROPERTY) {
            if (node_mapping.find(rec.node_id) != node_mapping.end()) {
                ge.update_property(node_mapping[rec.node_id]->id, rec.key, rec.val, rec.txn_id);
            }
        } else if (rec.op_type == WalOpType::DELETE_NODE) {
            if (node_mapping.find(rec.node_id) != node_mapping.end()) {
                ge.delete_node(node_mapping[rec.node_id]->id);
                // BUG: We intentionally do not call node_mapping.erase(rec.node_id).
                // This leaves a dangling pointer in node_mapping, leading to Use-After-Free
                // when subsequent WAL records (like UPDATE_PROPERTY) refer to this node_id.
            }
        }
        next_lsn_ = rec.lsn + 1;
    }
    return true;
}

void WalManager::create_checkpoint(GraphEngine& ge, const std::string& checkpoint_file) {
    force_flush();
    std::ofstream out(checkpoint_file, std::ios::binary);
    if (!out) return;

    // Checkpoint records current LSN
    out.write(reinterpret_cast<const char*>(&next_lsn_), 8);
    
    // Dump node mappings and current state
    const auto& nodes = ge.get_all_nodes();
    uint32_t node_count = static_cast<uint32_t>(nodes.size());
    out.write(reinterpret_cast<const char*>(&node_count), 4);

    for (const auto& [nid, n] : nodes) {
        out.write(reinterpret_cast<const char*>(&n->id), 4);
        uint32_t l_len = static_cast<uint32_t>(n->label.size());
        out.write(reinterpret_cast<const char*>(&l_len), 4);
        out.write(n->label.data(), l_len);

        uint32_t p_count = static_cast<uint32_t>(n->properties.size());
        out.write(reinterpret_cast<const char*>(&p_count), 4);
        for (const auto& [k, v] : n->properties) {
            uint32_t k_len = static_cast<uint32_t>(k.size());
            out.write(reinterpret_cast<const char*>(&k_len), 4);
            out.write(k.data(), k_len);
            
            uint8_t vt = static_cast<uint8_t>(v.type);
            out.write(reinterpret_cast<const char*>(&vt), 1);
            if (v.type == DataType::INT) {
                int32_t val = v.get_int();
                out.write(reinterpret_cast<const char*>(&val), 4);
            } else if (v.type == DataType::FLOAT) {
                float val = v.get_float();
                out.write(reinterpret_cast<const char*>(&val), 4);
            } else if (v.type == DataType::STRING) {
                std::string s = v.get_string();
                uint32_t v_len = static_cast<uint32_t>(s.size());
                out.write(reinterpret_cast<const char*>(&v_len), 4);
                out.write(s.data(), v_len);
            }
        }
    }
}

bool WalManager::restore_from_checkpoint(GraphEngine& ge, const std::string& checkpoint_file) {
    std::ifstream in(checkpoint_file, std::ios::binary);
    if (!in) return false;

    ge.clear();
    in.read(reinterpret_cast<char*>(&next_lsn_), 8);

    uint32_t node_count;
    in.read(reinterpret_cast<char*>(&node_count), 4);

    std::unordered_map<node_id_t, Node*> node_mapping;

    for (uint32_t i = 0; i < node_count; ++i) {
        node_id_t nid;
        in.read(reinterpret_cast<char*>(&nid), 4);

        uint32_t l_len;
        in.read(reinterpret_cast<char*>(&l_len), 4);
        std::string label;
        label.resize(l_len);
        in.read(&label[0], l_len);

        Node* created = ge.create_node(label);
        node_mapping[nid] = created;

        uint32_t p_count;
        in.read(reinterpret_cast<char*>(&p_count), 4);
        for (uint32_t p = 0; p < p_count; ++p) {
            uint32_t k_len;
            in.read(reinterpret_cast<char*>(&k_len), 4);
            std::string key;
            key.resize(k_len);
            in.read(&key[0], k_len);

            uint8_t vt;
            in.read(reinterpret_cast<char*>(&vt), 1);
            DataType dt = static_cast<DataType>(vt);
            if (dt == DataType::INT) {
                int32_t val;
                in.read(reinterpret_cast<char*>(&val), 4);
                created->properties[key] = Variant(val);
            } else if (dt == DataType::FLOAT) {
                float val;
                in.read(reinterpret_cast<char*>(&val), 4);
                created->properties[key] = Variant(val);
            } else if (dt == DataType::STRING) {
                uint32_t v_len;
                in.read(reinterpret_cast<char*>(&v_len), 4);
                std::string s;
                s.resize(v_len);
                in.read(&s[0], v_len);
                created->properties[key] = Variant(s);
            }
        }
    }
    return true;
}

void WalManager::truncate_before(uint64_t lsn) {
    // Truncates log file physically up to LSN by rewriting remaining records
    std::ifstream in(log_filename_, std::ios::binary);
    if (!in) return;

    std::vector<WalRecord> kept;
    while (in.peek() != EOF) {
        WalRecord rec;
        in.read(reinterpret_cast<char*>(&rec.lsn), 8);
        in.read(reinterpret_cast<char*>(&rec.txn_id), 4);
        uint8_t op;
        in.read(reinterpret_cast<char*>(&op), 1);
        rec.op_type = static_cast<WalOpType>(op);
        in.read(reinterpret_cast<char*>(&rec.node_id), 4);
        in.read(reinterpret_cast<char*>(&rec.edge_id), 4);

        uint32_t s_len;
        in.read(reinterpret_cast<char*>(&s_len), 4);
        rec.label_or_type.resize(s_len);
        in.read(&rec.label_or_type[0], s_len);

        uint32_t k_len;
        in.read(reinterpret_cast<char*>(&k_len), 4);
        rec.key.resize(k_len);
        in.read(&rec.key[0], k_len);

        uint8_t vt;
        in.read(reinterpret_cast<char*>(&vt), 1);
        DataType dt = static_cast<DataType>(vt);
        if (dt == DataType::INT) {
            int32_t val;
            in.read(reinterpret_cast<char*>(&val), 4);
            rec.val = Variant(val);
        } else if (dt == DataType::FLOAT) {
            float val;
            in.read(reinterpret_cast<char*>(&val), 4);
            rec.val = Variant(val);
        } else if (dt == DataType::STRING) {
            uint32_t v_len;
            in.read(reinterpret_cast<char*>(&v_len), 4);
            std::string s;
            s.resize(v_len);
            in.read(&s[0], v_len);
            rec.val = Variant(s);
        } else if (dt == DataType::VECTOR) {
            uint32_t v_len;
            in.read(reinterpret_cast<char*>(&v_len), 4);
            std::vector<float> vec;
            for (uint32_t v_idx = 0; v_idx < v_len; ++v_idx) {
                float val;
                in.read(reinterpret_cast<char*>(&val), 4);
                vec.push_back(val);
            }
            rec.val = Variant(vec);
        }
        in.read(reinterpret_cast<char*>(&rec.crc), 4);

        if (rec.lsn >= lsn) {
            kept.push_back(rec);
        }
    }
    in.close();

    std::ofstream out(log_filename_, std::ios::binary | std::ios::trunc);
    for (const auto& rec : kept) {
        out.write(reinterpret_cast<const char*>(&rec.lsn), 8);
        out.write(reinterpret_cast<const char*>(&rec.txn_id), 4);
        uint8_t op = static_cast<uint8_t>(rec.op_type);
        out.write(reinterpret_cast<const char*>(&op), 1);
        out.write(reinterpret_cast<const char*>(&rec.node_id), 4);
        out.write(reinterpret_cast<const char*>(&rec.edge_id), 4);
        
        uint32_t s_len = static_cast<uint32_t>(rec.label_or_type.size());
        out.write(reinterpret_cast<const char*>(&s_len), 4);
        out.write(rec.label_or_type.data(), s_len);

        uint32_t k_len = static_cast<uint32_t>(rec.key.size());
        out.write(reinterpret_cast<const char*>(&k_len), 4);
        out.write(rec.key.data(), k_len);

        uint8_t vt = static_cast<uint8_t>(rec.val.type);
        out.write(reinterpret_cast<const char*>(&vt), 1);
        if (rec.val.type == DataType::INT) {
            int32_t val = rec.val.get_int();
            out.write(reinterpret_cast<const char*>(&val), 4);
        } else if (rec.val.type == DataType::FLOAT) {
            float val = rec.val.get_float();
            out.write(reinterpret_cast<const char*>(&val), 4);
        } else if (rec.val.type == DataType::STRING) {
            std::string s = rec.val.get_string();
            uint32_t v_len = static_cast<uint32_t>(s.size());
            out.write(reinterpret_cast<const char*>(&v_len), 4);
            out.write(s.data(), v_len);
        }
        out.write(reinterpret_cast<const char*>(&rec.crc), 4);
    }
}

} // namespace AetherGraph
