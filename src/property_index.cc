#include "property_index.h"
#include <algorithm>

namespace AetherGraph {

PropertyIndex::PropertyIndex(const std::string& property_key, size_t min_degree)
    : min_degree_(min_degree), property_key_(property_key) {
    root_ = std::make_unique<IndexBTreeNode>(true);
}

void PropertyIndex::insert(const std::string& key, node_id_t value) {
    IndexBTreeNode* r = root_.get();
    if (r->keys.size() == 2 * min_degree_ - 1) {
        auto new_root = std::make_unique<IndexBTreeNode>(false);
        new_root->children.push_back(std::move(root_));
        root_ = std::move(new_root);
        split_child(root_.get(), 0, root_->children[0].get());
        insert_non_full(root_.get(), key, value);
    } else {
        insert_non_full(r, key, value);
    }
}

void PropertyIndex::split_child(IndexBTreeNode* node, size_t i, IndexBTreeNode* child) {
    auto sibling = std::make_unique<IndexBTreeNode>(child->is_leaf);
    
    // Copy the last (min_degree - 1) keys of child to sibling
    sibling->keys.assign(child->keys.begin() + min_degree_, child->keys.end());
    child->keys.erase(child->keys.begin() + min_degree_ - 1, child->keys.end());

    if (child->is_leaf) {
        sibling->values.assign(child->values.begin() + min_degree_, child->values.end());
        child->values.erase(child->values.begin() + min_degree_ - 1, child->values.end());
    } else {
        sibling->children.reserve(child->children.size() - min_degree_);
        for (size_t j = min_degree_; j < child->children.size(); ++j) {
            sibling->children.push_back(std::move(child->children[j]));
        }
        child->children.erase(child->children.begin() + min_degree_, child->children.end());
    }

    node->children.insert(node->children.begin() + i + 1, std::move(sibling));
    node->keys.insert(node->keys.begin() + i, child->keys.back());
    node->values.insert(node->values.begin() + i, child->values.back());
}

void PropertyIndex::insert_non_full(IndexBTreeNode* node, const std::string& key, node_id_t value) {
    int idx = static_cast<int>(node->keys.size()) - 1;
    if (node->is_leaf) {
        while (idx >= 0 && node->keys[idx] > key) {
            idx--;
        }
        if (idx >= 0 && node->keys[idx] == key) {
            node->values[idx].push_back(value);
        } else {
            node->keys.insert(node->keys.begin() + idx + 1, key);
            std::vector<node_id_t> vals = {value};
            node->values.insert(node->values.begin() + idx + 1, vals);
        }
    } else {
        while (idx >= 0 && node->keys[idx] > key) {
            idx--;
        }
        idx++;
        if (node->children[idx]->keys.size() == 2 * min_degree_ - 1) {
            split_child(node, idx, node->children[idx].get());
            if (node->keys[idx] < key) {
                idx++;
            }
        }
        insert_non_full(node->children[idx].get(), key, value);
    }
}

std::vector<node_id_t> PropertyIndex::search(const std::string& key) {
    std::vector<node_id_t> results;
    search_recursive(root_.get(), key, results);
    return results;
}

void PropertyIndex::search_recursive(IndexBTreeNode* node, const std::string& key, std::vector<node_id_t>& results) {
    size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        i++;
    }
    if (i < node->keys.size() && key == node->keys[i]) {
        results = node->values[i];
        return;
    }
    if (node->is_leaf) {
        return;
    }
    search_recursive(node->children[i].get(), key, results);
}

std::vector<node_id_t> PropertyIndex::range_search(const std::string& min_key, const std::string& max_key) {
    std::vector<node_id_t> results;
    range_search_recursive(root_.get(), min_key, max_key, results);
    return results;
}

void PropertyIndex::range_search_recursive(IndexBTreeNode* node, const std::string& min_key, const std::string& max_key, std::vector<node_id_t>& results) {
    size_t i = 0;
    while (i < node->keys.size() && min_key > node->keys[i]) {
        i++;
    }
    while (i < node->keys.size() && node->keys[i] <= max_key) {
        if (!node->is_leaf) {
            range_search_recursive(node->children[i].get(), min_key, max_key, results);
        }
        results.insert(results.end(), node->values[i].begin(), node->values[i].end());
        i++;
    }
    if (!node->is_leaf) {
        range_search_recursive(node->children[i].get(), min_key, max_key, results);
    }
}

void PropertyIndex::remove(const std::string& key, node_id_t value) {
    remove_recursive(root_.get(), key, value);
}

void PropertyIndex::remove_recursive(IndexBTreeNode* node, const std::string& key, node_id_t value) {
    size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        i++;
    }

    if (i < node->keys.size() && key == node->keys[i]) {
        if (node->is_leaf) {
            auto& vals = node->values[i];
            auto vit = std::find(vals.begin(), vals.end(), value);
            if (vit != vals.end()) {
                vals.erase(vit);
            }
            if (vals.empty()) {
                node->keys.erase(node->keys.begin() + i);
                node->values.erase(node->values.begin() + i);
            }
        } else {
            // Internal node removal handles replacement from predecessor or successor leaf
            IndexBTreeNode* pred_child = node->children[i].get();
            if (pred_child->keys.size() >= min_degree_) {
                while (!pred_child->is_leaf) {
                    pred_child = pred_child->children.back().get();
                }
                node->keys[i] = pred_child->keys.back();
                node->values[i] = pred_child->values.back();
                remove_recursive(node->children[i].get(), pred_child->keys.back(), value);
            } else {
                IndexBTreeNode* succ_child = node->children[i + 1].get();
                if (succ_child->keys.size() >= min_degree_) {
                    while (!succ_child->is_leaf) {
                        succ_child = succ_child->children.front().get();
                    }
                    node->keys[i] = succ_child->keys.front();
                    node->values[i] = succ_child->values.front();
                    remove_recursive(node->children[i + 1].get(), succ_child->keys.front(), value);
                } else {
                    // Merge pred_child and succ_child
                    pred_child->keys.push_back(node->keys[i]);
                    pred_child->values.push_back(node->values[i]);
                    pred_child->keys.insert(pred_child->keys.end(), succ_child->keys.begin(), succ_child->keys.end());
                    pred_child->values.insert(pred_child->values.end(), succ_child->values.begin(), succ_child->values.end());
                    if (!pred_child->is_leaf) {
                        for (auto& child : succ_child->children) {
                            pred_child->children.push_back(std::move(child));
                        }
                    }
                    node->keys.erase(node->keys.begin() + i);
                    node->values.erase(node->values.begin() + i);
                    node->children.erase(node->children.begin() + i + 1);
                    remove_recursive(pred_child, key, value);
                }
            }
        }
    } else {
        if (node->is_leaf) return;
        bool last_child = (i == node->keys.size());
        if (node->children[i]->keys.size() < min_degree_) {
            // Fill child node to ensure min_degree requirements
            IndexBTreeNode* child = node->children[i].get();
            if (i > 0 && node->children[i - 1]->keys.size() >= min_degree_) {
                IndexBTreeNode* left_sibling = node->children[i - 1].get();
                child->keys.insert(child->keys.begin(), node->keys[i - 1]);
                child->values.insert(child->values.begin(), node->values[i - 1]);
                node->keys[i - 1] = left_sibling->keys.back();
                node->values[i - 1] = left_sibling->values.back();
                left_sibling->keys.pop_back();
                left_sibling->values.pop_back();
                if (!child->is_leaf) {
                    child->children.insert(child->children.begin(), std::move(left_sibling->children.back()));
                    left_sibling->children.pop_back();
                }
            } else if (i < node->keys.size() && node->children[i + 1]->keys.size() >= min_degree_) {
                IndexBTreeNode* right_sibling = node->children[i + 1].get();
                child->keys.push_back(node->keys[i]);
                child->values.push_back(node->values[i]);
                node->keys[i] = right_sibling->keys.front();
                node->values[i] = right_sibling->values.front();
                right_sibling->keys.erase(right_sibling->keys.begin());
                right_sibling->values.erase(right_sibling->values.begin());
                if (!child->is_leaf) {
                    child->children.push_back(std::move(right_sibling->children.front()));
                    right_sibling->children.erase(right_sibling->children.begin());
                }
            } else {
                if (i < node->keys.size()) {
                    IndexBTreeNode* right_sibling = node->children[i + 1].get();
                    child->keys.push_back(node->keys[i]);
                    child->values.push_back(node->values[i]);
                    child->keys.insert(child->keys.end(), right_sibling->keys.begin(), right_sibling->keys.end());
                    child->values.insert(child->values.end(), right_sibling->values.begin(), right_sibling->values.end());
                    if (!child->is_leaf) {
                        for (auto& sib_child : right_sibling->children) {
                            child->children.push_back(std::move(sib_child));
                        }
                    }
                    node->keys.erase(node->keys.begin() + i);
                    node->values.erase(node->values.begin() + i);
                    node->children.erase(node->children.begin() + i + 1);
                } else {
                    IndexBTreeNode* left_sibling = node->children[i - 1].get();
                    left_sibling->keys.push_back(node->keys[i - 1]);
                    left_sibling->values.push_back(node->values[i - 1]);
                    left_sibling->keys.insert(left_sibling->keys.end(), child->keys.begin(), child->keys.end());
                    left_sibling->values.insert(left_sibling->values.end(), child->values.begin(), child->values.end());
                    if (!left_sibling->is_leaf) {
                        for (auto& c_child : child->children) {
                            left_sibling->children.push_back(std::move(c_child));
                        }
                    }
                    node->keys.erase(node->keys.begin() + i - 1);
                    node->values.erase(node->values.begin() + i - 1);
                    node->children.erase(node->children.begin() + i);
                    child = left_sibling;
                }
            }
            if (last_child && i > node->keys.size()) {
                remove_recursive(node->children[i - 1].get(), key, value);
            } else {
                remove_recursive(node->children[i].get(), key, value);
            }
        } else {
            remove_recursive(node->children[i].get(), key, value);
        }
    }
}

void PropertyIndex::build_from_graph(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    for (const auto& [nid, n] : nodes) {
        auto it = n->properties.find(property_key_);
        if (it != n->properties.end()) {
            std::string key_str;
            if (it->second.type == DataType::INT) key_str = std::to_string(it->second.get_int());
            else if (it->second.type == DataType::FLOAT) key_str = std::to_string(it->second.get_float());
            else if (it->second.type == DataType::STRING) key_str = it->second.get_string();
            insert(key_str, n->id);
        }
    }
}

} // namespace AetherGraph
