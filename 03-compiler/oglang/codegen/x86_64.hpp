#pragma once

#include "../ir/ir.hpp"
#include "../analysis/liveness.hpp"

#include <string>
#include <unordered_map>

class X86Codegen {
public:
    // Emits the process entry point. Exactly one of these must appear in
    // the final program; it calls `entryFunction` and exits the process
    // with its return value via the exit syscall.
    std::string generateEntryPoint(const std::string& entryFunction);

    // Emits one function's label and body. Calling convention: up to four
    // integer arguments are passed in edi, esi, edx, ecx (a restricted
    // subset of the System V AMD64 ABI's integer argument registers);
    // the return value comes back in eax.
    std::string generate(
        const IRFunction& function,
        const std::unordered_map<ValueId, std::string>& allocation,
        const std::unordered_map<ValueId, LiveRange>& liveness
    );
};
