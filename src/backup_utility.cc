#include "backup_utility.h"
#include "graph_serializer.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace AetherGraph {

BackupUtility::BackupUtility(GraphEngine& ge, WalManager& wal, const std::string& backup_dir)
    : ge_(ge), wal_(wal), backup_dir_(backup_dir) {}

bool BackupUtility::create_full_backup(uint32_t backup_id) {
    std::string filename = backup_dir_ + "/backup_" + std::to_string(backup_id) + ".bin";
    std::vector<uint8_t> data = GraphSerializer::export_to_binary(ge_);
    
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();

    BackupMetadata meta;
    meta.backup_id = backup_id;
    meta.lsn = 0; // Baseline LSN
    meta.timestamp = "2026-06-29 12:00:00";
    meta.is_incremental = false;
    backups_.push_back(meta);

    return true;
}

bool BackupUtility::create_incremental_backup(uint32_t backup_id, uint64_t last_lsn) {
    // Save delta logs up to last_lsn
    std::string filename = backup_dir_ + "/backup_" + std::to_string(backup_id) + "_inc.bin";
    
    // We snapshot the current graph state as incremental representation
    std::vector<uint8_t> data = GraphSerializer::export_to_binary(ge_);
    
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();

    BackupMetadata meta;
    meta.backup_id = backup_id;
    meta.lsn = last_lsn;
    meta.timestamp = "2026-06-29 12:05:00";
    meta.is_incremental = true;
    backups_.push_back(meta);

    return true;
}

bool BackupUtility::restore_backup(uint32_t backup_id) {
    auto it = std::find_if(backups_.begin(), backups_.end(), [backup_id](const BackupMetadata& m) {
        return m.backup_id == backup_id;
    });

    if (it == backups_.end()) return false;

    std::string filename;
    if (it->is_incremental) {
        filename = backup_dir_ + "/backup_" + std::to_string(backup_id) + "_inc.bin";
    } else {
        filename = backup_dir_ + "/backup_" + std::to_string(backup_id) + ".bin";
    }

    std::ifstream in(filename, std::ios::binary | std::ios::ate);
    if (!in) return false;

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        bool ok = GraphSerializer::import_from_binary(ge_, buffer);
        if (ok && it->is_incremental) {
            // Replay WAL records from recovery LSN if needed
            wal_.recover(ge_);
        }
        return ok;
    }

    return false;
}

} // namespace AetherGraph
