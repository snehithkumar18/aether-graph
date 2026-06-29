#include "schema_validator.h"
#include <regex>

namespace AetherGraph {

RangeRule::RangeRule(float min_val, float max_val) : min_val_(min_val), max_val_(max_val) {}

bool RangeRule::validate(const Variant& val, std::string& err_msg) const {
    float f_val = 0.0f;
    if (val.type == DataType::INT) {
        f_val = static_cast<float>(val.get_int());
    } else if (val.type == DataType::FLOAT) {
        f_val = val.get_float();
    } else {
        err_msg = "Expected numeric type for range validation";
        return false;
    }

    if (f_val < min_val_ || f_val > max_val_) {
        err_msg = "Value " + std::to_string(f_val) + " out of bounds [" + std::to_string(min_val_) + ", " + std::to_string(max_val_) + "]";
        return false;
    }
    return true;
}

RegexRule::RegexRule(const std::string& pattern) : pattern_(pattern) {}

bool RegexRule::validate(const Variant& val, std::string& err_msg) const {
    if (val.type != DataType::STRING) {
        err_msg = "Expected string type for regex validation";
        return false;
    }

    std::string s_val = val.get_string();
    try {
        std::regex re(pattern_);
        if (!std::regex_match(s_val, re)) {
            err_msg = "Value '" + s_val + "' does not match pattern '" + pattern_ + "'";
            return false;
        }
    } catch (const std::regex_error&) {
        err_msg = "Invalid regex pattern: " + pattern_;
        return false;
    }
    return true;
}

bool NotEmptyRule::validate(const Variant& val, std::string& err_msg) const {
    if (val.type == DataType::STRING) {
        if (val.get_string().empty()) {
            err_msg = "String property must not be empty";
            return false;
        }
    } else if (val.type == DataType::VECTOR) {
        if (val.get_vector().empty()) {
            err_msg = "Vector property must not be empty";
            return false;
        }
    } else if (val.type == DataType::NIL) {
        err_msg = "Property must not be null";
        return false;
    }
    return true;
}

// ======================================================================
// SchemaValidator Implementation
// ======================================================================
void SchemaValidator::add_range_rule(const std::string& label, const std::string& key, float min_val, float max_val) {
    rules_[label][key].push_back(std::make_unique<RangeRule>(min_val, max_val));
}

void SchemaValidator::add_regex_rule(const std::string& label, const std::string& key, const std::string& pattern) {
    rules_[label][key].push_back(std::make_unique<RegexRule>(pattern));
}

void SchemaValidator::add_not_empty_rule(const std::string& label, const std::string& key) {
    rules_[label][key].push_back(std::make_unique<NotEmptyRule>());
}

bool SchemaValidator::validate_entity(
    const std::string& label, const std::string& key, const Variant& val, std::string& err_msg) const {

    auto label_it = rules_.find(label);
    if (label_it == rules_.end()) return true;

    auto key_it = label_it->second.find(key);
    if (key_it == label_it->second.end()) return true;

    for (const auto& rule : key_it->second) {
        if (!rule->validate(val, err_msg)) {
            return false;
        }
    }
    return true;
}

bool SchemaValidator::validate_node(const Node& node, std::vector<std::string>& errors) const {
    auto label_it = rules_.find(node.label);
    if (label_it == rules_.end()) return true; // No rules for this label

    bool all_valid = true;
    for (const auto& [key, rule_list] : label_it->second) {
        auto val_it = node.properties.find(key);
        if (val_it == node.properties.end()) {
            // Check if any rule demands presence (like NotEmptyRule)
            for (const auto& rule : rule_list) {
                if (dynamic_cast<NotEmptyRule*>(rule.get()) != nullptr) {
                    errors.push_back("Missing required property: " + key);
                    all_valid = false;
                    break;
                }
            }
            continue;
        }

        std::string err;
        for (const auto& rule : rule_list) {
            if (!rule->validate(val_it->second, err)) {
                errors.push_back("Property '" + key + "' error: " + err);
                all_valid = false;
            }
        }
    }
    return all_valid;
}

} // namespace AetherGraph
