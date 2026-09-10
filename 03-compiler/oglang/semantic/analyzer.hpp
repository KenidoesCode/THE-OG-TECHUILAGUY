#pragma once

#include "../ast/ast.hpp"

class SemanticAnalyzer {
public:
    void analyze(const Function& function);
};
