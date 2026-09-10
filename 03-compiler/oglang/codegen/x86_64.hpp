#pragma once

#include "../ir/ir.hpp"
#include <string>

class X86Codegen {
public:
    std::string generate(const IRFunction& function);
};
