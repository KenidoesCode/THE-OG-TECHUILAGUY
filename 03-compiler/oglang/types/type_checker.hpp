#pragma once

#include "../ast/ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct FunctionSignature {
    std::vector<std::string> paramTypes;
    std::string returnType;
};

// struct name -> (field name -> field type). Field declaration order
// isn't needed here (only IR lowering needs field order for byte
// offsets); the type checker only needs to answer "does this struct
// have a field of this name, and what's its type."
using StructTable =
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

// enum name -> (variant name -> ordinal). Ordinals aren't actually
// used by the type checker itself (every variant access just has type
// "i32"), but are kept here rather than a plain set so this table has
// the same shape IR lowering needs.
using EnumTable =
    std::unordered_map<std::string, std::unordered_map<std::string, int>>;

class TypeChecker {
public:
    // A module's own declarations, validated and collected (duplicate
    // names rejected, field/param/return types restricted to base
    // types) but not yet checked against any function body — the
    // building block both check() (single-file) and checkModule()
    // (multi-file) are built from, so there is exactly one
    // implementation of "what counts as a valid struct/enum/function
    // declaration," not two that could silently drift apart.
    struct ModuleSymbols {
        std::unordered_map<std::string, FunctionSignature> signatures;
        StructTable structs;
        EnumTable enums;
    };

    static ModuleSymbols collectModuleSymbols(const Program& program);

    // Type-checks an entire single-file program. Builds a signature
    // table first so functions may call each other regardless of
    // declaration order (including recursively and self-recursively).
    // Struct/enum declarations are collected the same way, before any
    // function body is checked. Rejects a program with any `import`
    // statement outright — a single-file Program has no module
    // context to resolve an import against; see checkModule() for the
    // multi-file path.
    void check(const Program& program);

    // Type-checks one module (one source file) of a multi-file
    // program. externalSignatures/externalStructs/externalEnums are
    // the qualified ("module.name") symbols this module's imports
    // make visible — the caller (the compiler driver) is responsible
    // for resolving which modules are imported and qualifying their
    // symbols; checkModule() only merges what it's given with this
    // module's own (unqualified) declarations and checks every
    // function body against the result. requireMain is true only for
    // the program's single entry module.
    void checkModule(
        const Program& moduleProgram,
        const std::unordered_map<std::string, FunctionSignature>& externalSignatures,
        const StructTable& externalStructs,
        const EnumTable& externalEnums,
        bool requireMain
    );

private:
    std::unordered_map<std::string, FunctionSignature> signatures;
    StructTable structs;
    EnumTable enums;
};
