#include "storage_compressor.h"
#include <queue>

namespace AetherGraph {

struct Compare {
    bool operator()(HuffmanNode* l, HuffmanNode* r) {
        return l->freq > r->freq;
    }
};

void StorageCompressor::generate_codes(HuffmanNode* root, const std::string& str) {
    if (!root) return;
    if (!root->left && !root->right) {
        codes_[root->ch] = str;
        decodes_[str] = root->ch;
    }
    generate_codes(root->left, str + "0");
    generate_codes(root->right, str + "1");
}

void StorageCompressor::free_tree(HuffmanNode* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    delete root;
}

void StorageCompressor::build_tree(const std::string& text) {
    std::unordered_map<char, size_t> freq;
    for (char ch : text) freq[ch]++;

    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, Compare> min_heap;
    for (auto pair : freq) {
        min_heap.push(new HuffmanNode(pair.first, pair.second));
    }

    while (min_heap.size() != 1) {
        auto left = min_heap.top(); min_heap.pop();
        auto right = min_heap.top(); min_heap.pop();
        auto top = new HuffmanNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;
        min_heap.push(top);
    }

    if (!min_heap.empty()) {
        root_ = min_heap.top();
        generate_codes(root_, "");
        free_tree(root_);
    }
}

std::vector<uint8_t> StorageCompressor::compress(const std::vector<uint8_t>& data) {
    std::string text(data.begin(), data.end());
    build_tree(text);

    std::string encoded_str = "";
    for (char ch : text) {
        encoded_str += codes_[ch];
    }

    std::vector<uint8_t> result;
    uint8_t current_byte = 0;
    int bit_count = 0;

    for (char bit : encoded_str) {
        current_byte <<= 1;
        if (bit == '1') {
            current_byte |= 1;
        }
        bit_count++;
        if (bit_count == 8) {
            result.push_back(current_byte);
            current_byte = 0;
            bit_count = 0;
        }
    }
    if (bit_count > 0) {
        current_byte <<= (8 - bit_count);
        result.push_back(current_byte);
    }

    return result;
}

std::vector<uint8_t> StorageCompressor::decompress(const std::vector<uint8_t>& compressed_data) {
    std::string current_code = "";
    std::vector<uint8_t> result;

    for (uint8_t byte : compressed_data) {
        for (int i = 7; i >= 0; --i) {
            char bit = ((byte >> i) & 1) ? '1' : '0';
            current_code += bit;
            auto it = decodes_.find(current_code);
            if (it != decodes_.end()) {
                result.push_back(static_cast<uint8_t>(it->second));
                current_code = "";
            }
        }
    }
    return result;
}

} // namespace AetherGraph
