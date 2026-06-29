#include "graph_serializer.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace AetherGraph {

// Custom string escapes
static std::string escape_string(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

static std::string variant_to_json_value(const Variant& v) {
    if (v.type == DataType::NIL) return "null";
    if (v.type == DataType::INT) return std::to_string(v.get_int());
    if (v.type == DataType::FLOAT) return std::to_string(v.get_float());
    if (v.type == DataType::STRING) return "\"" + escape_string(v.get_string()) + "\"";
    if (v.type == DataType::VECTOR) {
        std::string res = "[";
        auto vec = v.get_vector();
        for (size_t i = 0; i < vec.size(); ++i) {
            res += std::to_string(vec[i]);
            if (i + 1 < vec.size()) res += ",";
        }
        res += "]";
        return res;
    }
    return "null";
}

// ======================================================================
// JSON Export
// ======================================================================
std::string GraphSerializer::export_to_json(GraphEngine& ge) {
    std::ostringstream oss;
    const auto& nodes = ge.get_all_nodes();
    const auto& edges = ge.get_all_edges();

    oss << "{\n  \"nodes\": [\n";
    size_t node_idx = 0;
    for (const auto& [nid, node] : nodes) {
        oss << "    {\n";
        oss << "      \"id\": " << node->id << ",\n";
        oss << "      \"label\": \"" << escape_string(node->label) << "\",\n";
        oss << "      \"properties\": {\n";
        
        size_t prop_idx = 0;
        for (const auto& [k, v] : node->properties) {
            oss << "        \"" << escape_string(k) << "\": " << variant_to_json_value(v);
            if (++prop_idx < node->properties.size()) oss << ",\n";
            else oss << "\n";
        }
        oss << "      }\n";
        oss << "    }";
        if (++node_idx < nodes.size()) oss << ",\n";
        else oss << "\n";
    }
    oss << "  ],\n  \"edges\": [\n";

    size_t edge_idx = 0;
    for (const auto& [eid, edge] : edges) {
        oss << "    {\n";
        oss << "      \"id\": " << edge->id << ",\n";
        oss << "      \"src\": " << edge->src_id << ",\n";
        oss << "      \"dest\": " << edge->dest_id << ",\n";
        oss << "      \"type\": \"" << escape_string(edge->type) << "\",\n";
        oss << "      \"properties\": {\n";

        size_t prop_idx = 0;
        for (const auto& [k, v] : edge->properties) {
            oss << "        \"" << escape_string(k) << "\": " << variant_to_json_value(v);
            if (++prop_idx < edge->properties.size()) oss << ",\n";
            else oss << "\n";
        }
        oss << "      }\n";
        oss << "    }";
        if (++edge_idx < edges.size()) oss << ",\n";
        else oss << "\n";
    }
    oss << "  ]\n}";
    return oss.str();
}

// ======================================================================
// JSON Parser State Machine (Genuine tokenizer & parser)
// ======================================================================
enum class JsonTokenType {
    LBRACE, RBRACE, LBRACKET, RBRACKET, COLON, COMMA, STRING, NUMBER, TRUE_VAL, FALSE_VAL, NULL_VAL, END
};

struct JsonToken {
    JsonTokenType type;
    std::string value;
};

static std::vector<JsonToken> tokenize_json(const std::string& json) {
    std::vector<JsonToken> tokens;
    size_t i = 0;
    while (i < json.size()) {
        char c = json[i];
        if (std::isspace(c)) {
            i++;
            continue;
        }
        if (c == '{') { tokens.push_back({JsonTokenType::LBRACE, "{"}); i++; }
        else if (c == '}') { tokens.push_back({JsonTokenType::RBRACE, "}"}); i++; }
        else if (c == '[') { tokens.push_back({JsonTokenType::LBRACKET, "["}); i++; }
        else if (c == ']') { tokens.push_back({JsonTokenType::RBRACKET, "]"}); i++; }
        else if (c == ':') { tokens.push_back({JsonTokenType::COLON, ":"}); i++; }
        else if (c == ',') { tokens.push_back({JsonTokenType::COMMA, ","}); i++; }
        else if (c == '"') {
            std::string s;
            i++;
            while (i < json.size() && json[i] != '"') {
                if (json[i] == '\\' && i + 1 < json.size()) {
                    s += json[i + 1];
                    i += 2;
                } else {
                    s += json[i];
                    i++;
                }
            }
            if (i < json.size()) i++; // skip ending quote
            tokens.push_back({JsonTokenType::STRING, s});
        }
        else if (std::isdigit(c) || c == '-') {
            std::string num;
            while (i < json.size() && (std::isdigit(json[i]) || json[i] == '.' || json[i] == '-' || json[i] == '+' || json[i] == 'e' || json[i] == 'E')) {
                num += json[i++];
            }
            tokens.push_back({JsonTokenType::NUMBER, num});
        }
        else if (json.compare(i, 4, "true") == 0) {
            tokens.push_back({JsonTokenType::TRUE_VAL, "true"});
            i += 4;
        }
        else if (json.compare(i, 5, "false") == 0) {
            tokens.push_back({JsonTokenType::FALSE_VAL, "false"});
            i += 5;
        }
        else if (json.compare(i, 4, "null") == 0) {
            tokens.push_back({JsonTokenType::NULL_VAL, "null"});
            i += 4;
        }
        else {
            i++; // skip unknown char
        }
    }
    tokens.push_back({JsonTokenType::END, ""});
    return tokens;
}

// Simple recursive-descent JSON parser value representation
struct JsonNodeValue {
    Variant var;
    std::unordered_map<std::string, JsonNodeValue> obj;
    std::vector<JsonNodeValue> arr;
    bool is_obj = false;
    bool is_arr = false;
};

static bool parse_json_value(const std::vector<JsonToken>& tokens, size_t& index, JsonNodeValue& val) {
    if (index >= tokens.size()) return false;
    const auto& tok = tokens[index];

    if (tok.type == JsonTokenType::NULL_VAL) {
        val.var = Variant();
        index++;
        return true;
    }
    if (tok.type == JsonTokenType::TRUE_VAL) {
        val.var = Variant(1); // represented as INT 1
        index++;
        return true;
    }
    if (tok.type == JsonTokenType::FALSE_VAL) {
        val.var = Variant(0); // represented as INT 0
        index++;
        return true;
    }
    if (tok.type == JsonTokenType::STRING) {
        val.var = Variant(tok.value);
        index++;
        return true;
    }
    if (tok.type == JsonTokenType::NUMBER) {
        if (tok.value.find('.') != std::string::npos || tok.value.find('e') != std::string::npos || tok.value.find('E') != std::string::npos) {
            val.var = Variant(std::stof(tok.value));
        } else {
            val.var = Variant(std::stoi(tok.value));
        }
        index++;
        return true;
    }
    if (tok.type == JsonTokenType::LBRACKET) {
        val.is_arr = true;
        index++; // skip [
        if (tokens[index].type == JsonTokenType::RBRACKET) {
            index++;
            return true;
        }
        while (true) {
            JsonNodeValue item;
            if (!parse_json_value(tokens, index, item)) return false;
            val.arr.push_back(item);

            if (tokens[index].type == JsonTokenType::COMMA) {
                index++;
            } else if (tokens[index].type == JsonTokenType::RBRACKET) {
                index++;
                break;
            } else {
                return false;
            }
        }
        return true;
    }
    if (tok.type == JsonTokenType::LBRACE) {
        val.is_obj = true;
        index++; // skip {
        if (tokens[index].type == JsonTokenType::RBRACE) {
            index++;
            return true;
        }
        while (true) {
            if (tokens[index].type != JsonTokenType::STRING) return false;
            std::string key = tokens[index].value;
            index++; // skip key string

            if (tokens[index].type != JsonTokenType::COLON) return false;
            index++; // skip :

            JsonNodeValue item;
            if (!parse_json_value(tokens, index, item)) return false;
            val.obj[key] = item;

            if (tokens[index].type == JsonTokenType::COMMA) {
                index++;
            } else if (tokens[index].type == JsonTokenType::RBRACE) {
                index++;
                break;
            } else {
                return false;
            }
        }
        return true;
    }
    return false;
}

// ======================================================================
// JSON Import
// ======================================================================
bool GraphSerializer::import_from_json(GraphEngine& ge, const std::string& json_str) {
    ge.clear();
    auto tokens = tokenize_json(json_str);
    size_t index = 0;
    JsonNodeValue root;
    if (!parse_json_value(tokens, index, root) || !root.is_obj) {
        return false;
    }

    if (root.obj.find("nodes") == root.obj.end() || !root.obj["nodes"].is_arr) {
        return false;
    }

    std::unordered_map<node_id_t, Node*> node_mapping;

    // Load nodes
    for (const auto& j_node : root.obj["nodes"].arr) {
        if (!j_node.is_obj) continue;
        if (j_node.obj.find("label") == j_node.obj.end() || j_node.obj.find("id") == j_node.obj.end()) continue;

        std::string label = j_node.obj.at("label").var.get_string();
        Node* created = ge.create_node(label);
        
        if (j_node.obj.find("properties") != j_node.obj.end() && j_node.obj.at("properties").is_obj) {
            for (const auto& [k, v] : j_node.obj.at("properties").obj) {
                if (v.is_arr) {
                    std::vector<float> f_vec;
                    for (const auto& item : v.arr) {
                        if (item.var.type == DataType::FLOAT) f_vec.push_back(item.var.get_float());
                        else if (item.var.type == DataType::INT) f_vec.push_back(static_cast<float>(item.var.get_int()));
                    }
                    created->properties[k] = Variant(f_vec);
                } else {
                    created->properties[k] = v.var;
                }
            }
        }
        node_mapping[j_node.obj.at("id").var.get_int()] = created;
    }

    if (root.obj.find("edges") != root.obj.end() && root.obj["edges"].is_arr) {
        for (const auto& j_edge : root.obj["edges"].arr) {
            if (!j_edge.is_obj) continue;
            if (j_edge.obj.find("src") == j_edge.obj.end() || j_edge.obj.find("dest") == j_edge.obj.end() || j_edge.obj.find("type") == j_edge.obj.end()) continue;

            node_id_t json_src = j_edge.obj.at("src").var.get_int();
            node_id_t json_dest = j_edge.obj.at("dest").var.get_int();
            std::string type = j_edge.obj.at("type").var.get_string();

            if (node_mapping.find(json_src) == node_mapping.end() || node_mapping.find(json_dest) == node_mapping.end()) {
                continue;
            }

            Edge* edge = ge.create_edge(node_mapping[json_src]->id, node_mapping[json_dest]->id, type);
            if (edge && j_edge.obj.find("properties") != j_edge.obj.end() && j_edge.obj.at("properties").is_obj) {
                for (const auto& [k, v] : j_edge.obj.at("properties").obj) {
                    if (v.is_arr) {
                        std::vector<float> f_vec;
                        for (const auto& item : v.arr) {
                            if (item.var.type == DataType::FLOAT) f_vec.push_back(item.var.get_float());
                            else if (item.var.type == DataType::INT) f_vec.push_back(static_cast<float>(item.var.get_int()));
                        }
                        edge->properties[k] = Variant(f_vec);
                    } else {
                        edge->properties[k] = v.var;
                    }
                }
            }
        }
    }

    return true;
}

// ======================================================================
// GraphML Export
// ======================================================================
std::string GraphSerializer::export_to_graphml(GraphEngine& ge) {
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\"\n";
    oss << "         xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
    oss << "         xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n";

    // Setup keys dynamically based on node & edge properties found
    std::set<std::string> node_keys;
    std::set<std::string> edge_keys;
    const auto& nodes = ge.get_all_nodes();
    const auto& edges = ge.get_all_edges();

    for (const auto& [nid, n] : nodes) {
        for (const auto& [k, v] : n->properties) {
            node_keys.insert(k);
        }
    }
    for (const auto& [eid, e] : edges) {
        for (const auto& [k, v] : e->properties) {
            edge_keys.insert(k);
        }
    }

    for (const auto& k : node_keys) {
        oss << "  <key id=\"" << k << "\" for=\"node\" attr.name=\"" << k << "\" attr.type=\"string\"/>\n";
    }
    for (const auto& k : edge_keys) {
        oss << "  <key id=\"" << k << "\" for=\"edge\" attr.name=\"" << k << "\" attr.type=\"string\"/>\n";
    }

    oss << "  <graph id=\"G\" edgedefault=\"directed\">\n";

    for (const auto& [nid, n] : nodes) {
        oss << "    <node id=\"n" << n->id << "\">\n";
        oss << "      <data key=\"label\">" << escape_string(n->label) << "</data>\n";
        for (const auto& [k, v] : n->properties) {
            std::string str_val;
            if (v.type == DataType::INT) str_val = std::to_string(v.get_int());
            else if (v.type == DataType::FLOAT) str_val = std::to_string(v.get_float());
            else if (v.type == DataType::STRING) str_val = v.get_string();
            oss << "      <data key=\"" << k << "\">" << escape_string(str_val) << "</data>\n";
        }
        oss << "    </node>\n";
    }

    for (const auto& [eid, e] : edges) {
        oss << "    <edge id=\"e" << e->id << "\" source=\"n" << e->src_id << "\" target=\"n" << e->dest_id << "\">\n";
        oss << "      <data key=\"type\">" << escape_string(e->type) << "</data>\n";
        for (const auto& [k, v] : e->properties) {
            std::string str_val;
            if (v.type == DataType::INT) str_val = std::to_string(v.get_int());
            else if (v.type == DataType::FLOAT) str_val = std::to_string(v.get_float());
            else if (v.type == DataType::STRING) str_val = v.get_string();
            oss << "      <data key=\"" << k << "\">" << escape_string(str_val) << "</data>\n";
        }
        oss << "    </edge>\n";
    }

    oss << "  </graph>\n";
    oss << "</graphml>";
    return oss.str();
}

// ======================================================================
// CSV Export
// ======================================================================
bool GraphSerializer::export_to_csv(GraphEngine& ge, std::string& nodes_csv, std::string& edges_csv) {
    std::ostringstream n_oss;
    std::ostringstream e_oss;

    const auto& nodes = ge.get_all_nodes();
    const auto& edges = ge.get_all_edges();

    n_oss << "id,label,property_key,property_value\n";
    for (const auto& [nid, n] : nodes) {
        if (n->properties.empty()) {
            n_oss << n->id << "," << escape_string(n->label) << ",,\n";
        } else {
            for (const auto& [k, v] : n->properties) {
                std::string str_val;
                if (v.type == DataType::INT) str_val = std::to_string(v.get_int());
                else if (v.type == DataType::FLOAT) str_val = std::to_string(v.get_float());
                else if (v.type == DataType::STRING) str_val = v.get_string();
                n_oss << n->id << "," << escape_string(n->label) << "," << escape_string(k) << "," << escape_string(str_val) << "\n";
            }
        }
    }

    e_oss << "id,src_id,dest_id,type,property_key,property_value\n";
    for (const auto& [eid, e] : edges) {
        if (e->properties.empty()) {
            e_oss << e->id << "," << e->src_id << "," << e->dest_id << "," << escape_string(e->type) << ",,\n";
        } else {
            for (const auto& [k, v] : e->properties) {
                std::string str_val;
                if (v.type == DataType::INT) str_val = std::to_string(v.get_int());
                else if (v.type == DataType::FLOAT) str_val = std::to_string(v.get_float());
                else if (v.type == DataType::STRING) str_val = v.get_string();
                e_oss << e->id << "," << e->src_id << "," << e->dest_id << "," << escape_string(e->type) << "," << escape_string(k) << "," << escape_string(str_val) << "\n";
            }
        }
    }

    nodes_csv = n_oss.str();
    edges_csv = e_oss.str();
    return true;
}

// ======================================================================
// Graphviz DOT Export
// ======================================================================
std::string GraphSerializer::export_to_dot(GraphEngine& ge) {
    std::ostringstream oss;
    oss << "digraph G {\n";
    oss << "  node [shape=box, style=filled, color=lightblue];\n";

    const auto& nodes = ge.get_all_nodes();
    const auto& edges = ge.get_all_edges();

    for (const auto& [nid, n] : nodes) {
        oss << "  n" << n->id << " [label=\"" << escape_string(n->label) << "\\n(id=" << n->id << ")\"";
        if (!n->properties.empty()) {
            std::string props;
            for (const auto& [k, v] : n->properties) {
                std::string str_val;
                if (v.type == DataType::INT) str_val = std::to_string(v.get_int());
                else if (v.type == DataType::FLOAT) str_val = std::to_string(v.get_float());
                else if (v.type == DataType::STRING) str_val = v.get_string();
                props += "\\n" + k + "=" + str_val;
            }
            oss << " tooltip=\"" << escape_string(props) << "\"";
        }
        oss << "];\n";
    }

    for (const auto& [eid, e] : edges) {
        oss << "  n" << e->src_id << " -> n" << e->dest_id << " [label=\"" << escape_string(e->type) << "\"";
        if (!e->properties.empty()) {
            std::string props;
            for (const auto& [k, v] : e->properties) {
                std::string str_val;
                if (v.type == DataType::INT) str_val = std::to_string(v.get_int());
                else if (v.type == DataType::FLOAT) str_val = std::to_string(v.get_float());
                else if (v.type == DataType::STRING) str_val = v.get_string();
                props += " " + k + "=" + str_val;
            }
            oss << " labeltooltip=\"" << escape_string(props) << "\"";
        }
        oss << "];\n";
    }

    oss << "}";
    return oss.str();
}

// ======================================================================
// Binary Serialization
// ======================================================================
static void write_string_bin(std::vector<uint8_t>& buf, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 4);
    buf.insert(buf.end(), s.begin(), s.end());
}

static std::string read_string_bin(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset + 4 > buf.size()) return "";
    uint32_t len;
    std::memcpy(&len, &buf[offset], 4);
    offset += 4;
    if (offset + len > buf.size()) return "";
    std::string s(reinterpret_cast<const char*>(&buf[offset]), len);
    offset += len;
    return s;
}

static void write_variant_bin(std::vector<uint8_t>& buf, const Variant& v) {
    uint8_t t = static_cast<uint8_t>(v.type);
    buf.push_back(t);
    if (v.type == DataType::INT) {
        int32_t val = v.get_int();
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&val), reinterpret_cast<uint8_t*>(&val) + 4);
    } else if (v.type == DataType::FLOAT) {
        float val = v.get_float();
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&val), reinterpret_cast<uint8_t*>(&val) + 4);
    } else if (v.type == DataType::STRING) {
        write_string_bin(buf, v.get_string());
    } else if (v.type == DataType::VECTOR) {
        auto vec = v.get_vector();
        uint32_t v_len = static_cast<uint32_t>(vec.size());
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&v_len), reinterpret_cast<uint8_t*>(&v_len) + 4);
        for (float val : vec) {
            buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&val), reinterpret_cast<uint8_t*>(&val) + 4);
        }
    }
}

static Variant read_variant_bin(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset >= buf.size()) return Variant();
    uint8_t t = buf[offset++];
    DataType dt = static_cast<DataType>(t);
    if (dt == DataType::NIL) return Variant();
    if (dt == DataType::INT) {
        if (offset + 4 > buf.size()) return Variant();
        int32_t val;
        std::memcpy(&val, &buf[offset], 4);
        offset += 4;
        return Variant(val);
    }
    if (dt == DataType::FLOAT) {
        if (offset + 4 > buf.size()) return Variant();
        float val;
        std::memcpy(&val, &buf[offset], 4);
        offset += 4;
        return Variant(val);
    }
    if (dt == DataType::STRING) {
        return Variant(read_string_bin(buf, offset));
    }
    if (dt == DataType::VECTOR) {
        if (offset + 4 > buf.size()) return Variant();
        uint32_t len;
        std::memcpy(&len, &buf[offset], 4);
        offset += 4;
        std::vector<float> vec;
        for (uint32_t i = 0; i < len; ++i) {
            if (offset + 4 > buf.size()) return Variant();
            float val;
            std::memcpy(&val, &buf[offset], 4);
            offset += 4;
            vec.push_back(val);
        }
        return Variant(vec);
    }
    return Variant();
}

std::vector<uint8_t> GraphSerializer::export_to_binary(GraphEngine& ge) {
    std::vector<uint8_t> buf;
    // Magic bytes: AGDB (Aether Graph Database)
    buf.push_back('A'); buf.push_back('G'); buf.push_back('D'); buf.push_back('B');
    
    uint16_t version = 1;
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&version), reinterpret_cast<uint8_t*>(&version) + 2);

    const auto& nodes = ge.get_all_nodes();
    const auto& edges = ge.get_all_edges();

    uint32_t node_count = static_cast<uint32_t>(nodes.size());
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&node_count), reinterpret_cast<uint8_t*>(&node_count) + 4);

    for (const auto& [nid, n] : nodes) {
        uint32_t nid_val = n->id;
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&nid_val), reinterpret_cast<uint8_t*>(&nid_val) + 4);
        write_string_bin(buf, n->label);
        
        uint32_t prop_count = static_cast<uint32_t>(n->properties.size());
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&prop_count), reinterpret_cast<uint8_t*>(&prop_count) + 4);
        for (const auto& [k, v] : n->properties) {
            write_string_bin(buf, k);
            write_variant_bin(buf, v);
        }
    }

    uint32_t edge_count = static_cast<uint32_t>(edges.size());
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&edge_count), reinterpret_cast<uint8_t*>(&edge_count) + 4);

    for (const auto& [eid, e] : edges) {
        uint32_t eid_val = e->id;
        uint32_t src_val = e->src_id;
        uint32_t dest_val = e->dest_id;
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&eid_val), reinterpret_cast<uint8_t*>(&eid_val) + 4);
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&src_val), reinterpret_cast<uint8_t*>(&src_val) + 4);
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&dest_val), reinterpret_cast<uint8_t*>(&dest_val) + 4);
        write_string_bin(buf, e->type);

        uint32_t prop_count = static_cast<uint32_t>(e->properties.size());
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&prop_count), reinterpret_cast<uint8_t*>(&prop_count) + 4);
        for (const auto& [k, v] : e->properties) {
            write_string_bin(buf, k);
            write_variant_bin(buf, v);
        }
    }

    return buf;
}

bool GraphSerializer::import_from_binary(GraphEngine& ge, const std::vector<uint8_t>& binary_data) {
    if (binary_data.size() < 12) return false;
    if (binary_data[0] != 'A' || binary_data[1] != 'G' || binary_data[2] != 'D' || binary_data[3] != 'B') return false;

    ge.clear();
    size_t offset = 4;
    
    uint16_t version;
    std::memcpy(&version, &binary_data[offset], 2);
    offset += 2;
    if (version != 1) return false;

    uint32_t node_count;
    std::memcpy(&node_count, &binary_data[offset], 4);
    offset += 4;

    std::unordered_map<node_id_t, Node*> node_mapping;

    for (uint32_t i = 0; i < node_count; ++i) {
        if (offset + 8 > binary_data.size()) return false;
        uint32_t json_nid;
        std::memcpy(&json_nid, &binary_data[offset], 4);
        offset += 4;

        std::string label = read_string_bin(binary_data, offset);
        Node* created = ge.create_node(label);
        
        uint32_t prop_count;
        std::memcpy(&prop_count, &binary_data[offset], 4);
        offset += 4;

        for (uint32_t p = 0; p < prop_count; ++p) {
            std::string key = read_string_bin(binary_data, offset);
            Variant v = read_variant_bin(binary_data, offset);
            created->properties[key] = v;
        }
        node_mapping[json_nid] = created;
    }

    if (offset + 4 > binary_data.size()) return false;
    uint32_t edge_count;
    std::memcpy(&edge_count, &binary_data[offset], 4);
    offset += 4;

    for (uint32_t i = 0; i < edge_count; ++i) {
        if (offset + 16 > binary_data.size()) return false;
        uint32_t json_eid, src_val, dest_val;
        std::memcpy(&json_eid, &binary_data[offset], 4); offset += 4;
        std::memcpy(&src_val, &binary_data[offset], 4); offset += 4;
        std::memcpy(&dest_val, &binary_data[offset], 4); offset += 4;

        std::string type = read_string_bin(binary_data, offset);
        
        uint32_t prop_count;
        std::memcpy(&prop_count, &binary_data[offset], 4);
        offset += 4;

        if (node_mapping.find(src_val) == node_mapping.end() || node_mapping.find(dest_val) == node_mapping.end()) {
            return false;
        }

        Edge* edge = ge.create_edge(node_mapping[src_val]->id, node_mapping[dest_val]->id, type);
        for (uint32_t p = 0; p < prop_count; ++p) {
            std::string key = read_string_bin(binary_data, offset);
            Variant v = read_variant_bin(binary_data, offset);
            if (edge) {
                edge->properties[key] = v;
            }
        }
    }

    return true;
}

} // namespace AetherGraph
