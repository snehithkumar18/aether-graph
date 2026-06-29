#include "cypher_parser.h"
#include <algorithm>
#include <cctype>

namespace AetherGraph {

std::vector<CypherToken> CypherParser::tokenize(const std::string& query) {
    std::vector<CypherToken> tokens;
    size_t i = 0;
    while (i < query.size()) {
        char c = query[i];
        if (std::isspace(c)) {
            i++;
            continue;
        }

        if (c == '(') { tokens.push_back({CypherTokenType::LPAREN, "("}); i++; }
        else if (c == ')') { tokens.push_back({CypherTokenType::RPAREN, ")"}); i++; }
        else if (c == '[') { tokens.push_back({CypherTokenType::LBRACKET, "["}); i++; }
        else if (c == ']') { tokens.push_back({CypherTokenType::RBRACKET, "]"}); i++; }
        else if (c == '{') { tokens.push_back({CypherTokenType::LBRACE, "{"}); i++; }
        else if (c == '}') { tokens.push_back({CypherTokenType::RBRACE, "}"}); i++; }
        else if (c == ':') { tokens.push_back({CypherTokenType::COLON, ":"}); i++; }
        else if (c == '.') { tokens.push_back({CypherTokenType::DOT, "."}); i++; }
        else if (c == ',') { tokens.push_back({CypherTokenType::COMMA, ","}); i++; }
        else if (c == '=') { tokens.push_back({CypherTokenType::EQUALS, "="}); i++; }
        else if (c == '-') {
            if (i + 2 < query.size() && query[i + 1] == '-' && query[i + 2] == '>') {
                tokens.push_back({CypherTokenType::ARROW, "-->"});
                i += 3;
            } else {
                tokens.push_back({CypherTokenType::DASH, "-"});
                i++;
            }
        }
        else if (c == '"' || c == '\'') {
            char quote = c;
            std::string s;
            i++;
            while (i < query.size() && query[i] != quote) {
                s += query[i++];
            }
            if (i < query.size()) i++;
            tokens.push_back({CypherTokenType::STRING_LIT, s});
        }
        else if (std::isdigit(c)) {
            std::string num;
            while (i < query.size() && (std::isdigit(query[i]) || query[i] == '.')) {
                num += query[i++];
            }
            tokens.push_back({CypherTokenType::NUMBER_LIT, num});
        }
        else if (std::isalpha(c) || c == '_') {
            std::string ident;
            while (i < query.size() && (std::isalnum(query[i]) || query[i] == '_')) {
                ident += query[i++];
            }
            
            std::string upper_ident = ident;
            std::transform(upper_ident.begin(), upper_ident.end(), upper_ident.begin(), ::toupper);

            if (upper_ident == "MATCH") tokens.push_back({CypherTokenType::MATCH, ident});
            else if (upper_ident == "WHERE") tokens.push_back({CypherTokenType::WHERE, ident});
            else if (upper_ident == "RETURN") tokens.push_back({CypherTokenType::RETURN, ident});
            else if (upper_ident == "CREATE") tokens.push_back({CypherTokenType::CREATE, ident});
            else if (upper_ident == "DELETE") tokens.push_back({CypherTokenType::DELETE_OP, ident});
            else if (upper_ident == "SET") tokens.push_back({CypherTokenType::SET, ident});
            else if (upper_ident == "AND") tokens.push_back({CypherTokenType::AND, ident});
            else if (upper_ident == "OR") tokens.push_back({CypherTokenType::OR, ident});
            else if (upper_ident == "NOT") tokens.push_back({CypherTokenType::NOT, ident});
            else tokens.push_back({CypherTokenType::IDENTIFIER, ident});
        }
        else {
            i++;
        }
    }
    tokens.push_back({CypherTokenType::EOF_TOK, ""});
    return tokens;
}

ASTStatement CypherParser::parse(const std::string& query) {
    auto tokens = tokenize(query);
    size_t i = 0;
    ASTStatement stmt;

    if (tokens[i].type == CypherTokenType::MATCH) {
        stmt.op = "MATCH";
        i++; // skip MATCH
        
        // Parse node pattern (n:Label)
        if (tokens[i].type == CypherTokenType::LPAREN) {
            i++; // skip (
            ASTPathPattern path;
            if (tokens[i].type == CypherTokenType::IDENTIFIER) {
                path.start.variable = tokens[i].value;
                i++;
            }
            if (tokens[i].type == CypherTokenType::COLON) {
                i++; // skip :
                if (tokens[i].type == CypherTokenType::IDENTIFIER) {
                    path.start.label = tokens[i].value;
                    i++;
                }
            }
            if (tokens[i].type == CypherTokenType::RPAREN) {
                i++; // skip )
            }
            stmt.patterns.push_back(path);
        }

        // Parse optional WHERE clause
        if (tokens[i].type == CypherTokenType::WHERE) {
            i++; // skip WHERE
            if (tokens[i].type == CypherTokenType::IDENTIFIER) {
                ASTPredicate pred;
                pred.variable = tokens[i].value;
                i++;
                if (tokens[i].type == CypherTokenType::DOT) {
                    i++; // skip .
                    if (tokens[i].type == CypherTokenType::IDENTIFIER) {
                        pred.property_key = tokens[i].value;
                        i++;
                    }
                }
                if (tokens[i].type == CypherTokenType::EQUALS) {
                    i++; // skip =
                    if (tokens[i].type == CypherTokenType::STRING_LIT) {
                        pred.val = Variant(tokens[i].value);
                        i++;
                    } else if (tokens[i].type == CypherTokenType::NUMBER_LIT) {
                        if (tokens[i].value.find('.') != std::string::npos) {
                            pred.val = Variant(std::stof(tokens[i].value));
                        } else {
                            pred.val = Variant(std::stoi(tokens[i].value));
                        }
                        i++;
                    }
                }
                stmt.predicates.push_back(pred);
            }
        }

        // Parse optional RETURN clause
        if (tokens[i].type == CypherTokenType::RETURN) {
            i++; // skip RETURN
            while (tokens[i].type == CypherTokenType::IDENTIFIER) {
                stmt.return_vars.push_back(tokens[i].value);
                i++;
                if (tokens[i].type == CypherTokenType::COMMA) {
                    i++; // skip ,
                }
            }
        }
    } else if (tokens[i].type == CypherTokenType::CREATE) {
        stmt.op = "CREATE";
        i++; // skip CREATE
        if (tokens[i].type == CypherTokenType::LPAREN) {
            i++; // skip (
            ASTPathPattern path;
            if (tokens[i].type == CypherTokenType::IDENTIFIER) {
                path.start.variable = tokens[i].value;
                i++;
            }
            if (tokens[i].type == CypherTokenType::COLON) {
                i++; // skip :
                if (tokens[i].type == CypherTokenType::IDENTIFIER) {
                    path.start.label = tokens[i].value;
                    i++;
                }
            }
            if (tokens[i].type == CypherTokenType::RPAREN) {
                i++; // skip )
            }
            stmt.patterns.push_back(path);
        }
    }

    return stmt;
}

} // namespace AetherGraph
