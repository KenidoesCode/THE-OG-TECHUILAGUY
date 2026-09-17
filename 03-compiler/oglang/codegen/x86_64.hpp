#pragma once

#include "../ir/ir.hpp"
#include "../analysis/liveness.hpp"
#include "register_allocator.hpp"

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
    //
    // Every function gets a standard rbp-based frame (push rbp; mov
    // rsp,rbp; sub $N,rsp if it has spill slots) so spilled values sit
    // at fixed rbp-relative offsets regardless of how much the balanced
    // pushq/popq churn from call-argument marshaling or division
    // scratch-saving transiently moves rsp elsewhere in the body.
    std::string generate(
        const IRFunction& function,
        const RegisterAllocation& allocation,
        const std::unordered_map<ValueId, LiveRange>& liveness
    );
};
