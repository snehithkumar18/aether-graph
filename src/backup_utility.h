#ifndef AETHER_GRAPH_BACKUP_UTILITY_H
#define AETHER_GRAPH_BACKUP_UTILITY_H

#include "graph_engine.h"
#include "wal_manager.h"
#include <string>
#include <vector>

namespace AetherGraph {

struct BackupMetadata {
    uint32_t backup_id;
    uint64_t lsn;
    std::string timestamp;
    bool is_incremental;
};

class BackupUtility {
private:
    GraphEngine& ge_;
    WalManager& wal_;
    std::string backup_dir_;
    std::vector<BackupMetadata> backups_;

public:
    BackupUtility(GraphEngine& ge, WalManager& wal, const std::string& backup_dir);
    ~BackupUtility() = default;

    bool create_full_backup(uint32_t backup_id);
    bool create_incremental_backup(uint32_t backup_id, uint64_t last_lsn);
    
    bool restore_backup(uint32_t backup_id);
    const std::vector<BackupMetadata>& get_available_backups() const { return backups_; }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_BACKUP_UTILITY_H
