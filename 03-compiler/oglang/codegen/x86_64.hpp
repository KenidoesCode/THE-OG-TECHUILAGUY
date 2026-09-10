#pragma once

#include "../ir/ir.hpp"
#include <string>
#include <unordered_map>

class X86Codegen {
public:
    std::string generate(
        const IRFunction& function,
        const std::unordered_map<ValueId, std::string>& allocation
    );
};
