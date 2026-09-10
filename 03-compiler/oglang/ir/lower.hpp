#pragma once

#include "../ast/ast.hpp"
#include "ir.hpp"

class IRLowerer {
public:
    IRFunction lower(const Function& function);
};
