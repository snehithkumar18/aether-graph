#ifndef AETHER_GRAPH_SCHEMA_VALIDATOR_H
#define AETHER_GRAPH_SCHEMA_VALIDATOR_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace AetherGraph {

class ValidationRule {
public:
    virtual ~ValidationRule() = default;
    virtual bool validate(const Variant& val, std::string& err_msg) const = 0;
};

class RangeRule : public ValidationRule {
private:
    float min_val_;
    float max_val_;

public:
    RangeRule(float min_val, float max_val);
    bool validate(const Variant& val, std::string& err_msg) const override;
};

class RegexRule : public ValidationRule {
private:
    std::string pattern_;

public:
    explicit RegexRule(const std::string& pattern);
    bool validate(const Variant& val, std::string& err_msg) const override;
};

class NotEmptyRule : public ValidationRule {
public:
    NotEmptyRule() = default;
    bool validate(const Variant& val, std::string& err_msg) const override;
};

class SchemaValidator {
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::unique_ptr<ValidationRule>>>> rules_;

public:
    SchemaValidator() = default;
    ~SchemaValidator() = default;

    void add_range_rule(const std::string& label, const std::string& key, float min_val, float max_val);
    void add_regex_rule(const std::string& label, const std::string& key, const std::string& pattern);
    void add_not_empty_rule(const std::string& label, const std::string& key);

    bool validate_entity(const std::string& label, const std::string& key, const Variant& val, std::string& err_msg) const;
    bool validate_node(const Node& node, std::vector<std::string>& errors) const;
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_SCHEMA_VALIDATOR_H
