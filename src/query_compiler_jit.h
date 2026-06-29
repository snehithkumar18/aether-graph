#ifndef AETHER_GRAPH_QUERY_COMPILER_JIT_H
#define AETHER_GRAPH_QUERY_COMPILER_JIT_H

#include "execution_plans.h"
#include <vector>
#include <string>
#include <memory>

namespace AetherGraph {

enum class Opcode {
    LOAD_NODE,
    GET_PROPERTY,
    COMPARE_INT,
    COMPARE_STR,
    JUMP_IF_FALSE,
    YIELD_RESULT,
    HALT
};

struct Instruction {
    Opcode op;
    int32_t arg1;
    int32_t arg2;
    std::string str_arg;
    const Variant* cached_val_ptr = nullptr;
};

class QueryCompilerJIT {
private:
    std::vector<Instruction> bytecode_;

public:
    QueryCompilerJIT() = default;
    ~QueryCompilerJIT() = default;

    void compile_operator(GraphEngine& ge, const PhysicalOperator* op);
    const std::vector<Instruction>& get_bytecode() const { return bytecode_; }
    
    bool execute(GraphEngine& ge, std::vector<Node*>& output);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_COMPILER_JIT_H
