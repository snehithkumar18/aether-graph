#ifndef AETHER_GRAPH_CYPHER_PARSER_H
#define AETHER_GRAPH_CYPHER_PARSER_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <memory>

namespace AetherGraph {

enum class CypherTokenType {
    MATCH, WHERE, RETURN, CREATE, DELETE_OP, SET, IDENTIFIER, STRING_LIT, NUMBER_LIT,
    LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE, COLON, DOT, COMMA, EQUALS,
    ARROW, DASH, AND, OR, NOT, EOF_TOK
};

struct CypherToken {
    CypherTokenType type;
    std::string value;
};

// Abstract Syntax Tree Nodes
struct ASTPatternNode {
    std::string variable;
    std::string label;
};

struct ASTPatternRelationship {
    std::string variable;
    std::string type;
    bool left_to_right;
};

struct ASTPathPattern {
    ASTPatternNode start;
    std::vector<std::pair<ASTPatternRelationship, ASTPatternNode>> hops;
};

struct ASTPredicate {
    std::string variable;
    std::string property_key;
    Variant val;
    bool is_equals = true;
};

struct ASTStatement {
    std::string op; // MATCH, CREATE, DELETE
    std::vector<ASTPathPattern> patterns;
    std::vector<ASTPredicate> predicates;
    std::vector<std::string> return_vars;
};

class CypherParser {
public:
    static std::vector<CypherToken> tokenize(const std::string& query);
    static ASTStatement parse(const std::string& query);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_CYPHER_PARSER_H
