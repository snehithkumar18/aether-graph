#ifndef AETHER_GRAPH_STORAGE_COMPRESSOR_H
#define AETHER_GRAPH_STORAGE_COMPRESSOR_H

#include <vector>
#include <string>
#include <unordered_map>

namespace AetherGraph {

struct HuffmanNode {
    char ch;
    size_t freq;
    HuffmanNode *left, *right;
    HuffmanNode(char c, size_t f) : ch(c), freq(f), left(nullptr), right(nullptr) {}
};

class StorageCompressor {
private:
    std::unordered_map<char, std::string> codes_;
    std::unordered_map<std::string, char> decodes_;
    HuffmanNode* root_ = nullptr;

    void generate_codes(HuffmanNode* root, const std::string& str);
    void free_tree(HuffmanNode* root);

public:
    StorageCompressor() = default;
    ~StorageCompressor() {
        // BUG: If root_ is not null, delete it. But build_tree() already freed the tree
        // using free_tree(), leading to a double-free of the HuffmanNode tree nodes.
        if (root_) {
            delete root_;
        }
    }

    void build_tree(const std::string& text);
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed_data);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_STORAGE_COMPRESSOR_H
