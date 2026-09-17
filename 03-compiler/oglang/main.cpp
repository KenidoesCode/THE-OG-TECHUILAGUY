#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/type_checker.hpp"
#include "ir/lower.hpp"
#include "analysis/liveness.hpp"
#include "analysis/interference.hpp"
#include "codegen/register_allocator.hpp"
#include "codegen/x86_64.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using StructLayouts =
    std::unordered_map<std::string, std::vector<std::string>>;
using EnumOrdinals =
    std::unordered_map<std::string, std::unordered_map<std::string, int>>;

std::string readFile(const std::string& path, bool& ok) {
    std::ifstream file(path);

    if (!file) {
        ok = false;
        return "";
    }

    ok = true;
    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

std::string replaceAll(
    std::string text,
    const std::string& from,
    const std::string& to
) {
    size_t pos = 0;

    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }

    return text;
}

// --------------------------------------------------------------
// Call-site mangling for multi-module codegen.
//
// Type checking works entirely in terms of OGLang-level names
// ("module.function", a compound string used as a map key — see
// docs/ADR/0002-oglang-modules.md). Those names are never touched
// there. Codegen, however, needs real assembly symbols, and a dotted
// name is not a safe or portable choice of symbol — this pass walks
// each module's own AST *after* type-checking has already succeeded
// and rewrites it in place:
//
//   - a qualified call ("colors.brightness") always becomes
//     "colors__brightness", regardless of which module the call site
//     lives in;
//   - within a non-entry module, the module's OWN function
//     declarations are renamed "thisModule__name", and every
//     unqualified call site within that same module (which can only
//     refer to one of its own functions — see checkModule()) is
//     renamed the same way, so the call and the declaration agree.
//
// The entry module (the first file on the command line) is left
// completely unmangled — its own declarations and its own unqualified
// call sites keep their plain, single-file-compatible names — so a
// program with exactly one module needs no rewriting pass at all in
// spirit (single-file compilation never calls into this code).
void mangleCallsInExpr(
    Expr& expr,
    const std::string& modulePrefix,
    bool mangleThisModule
) {
    if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        if (call->callee.find('.') != std::string::npos) {
            call->callee = replaceAll(call->callee, ".", "__");
        } else if (mangleThisModule) {
            call->callee = modulePrefix + "__" + call->callee;
        }

        for (auto& arg : call->args) {
            mangleCallsInExpr(*arg, modulePrefix, mangleThisModule);
        }

        return;
    }

    if (auto* binary = dynamic_cast<BinaryExpr*>(&expr)) {
        mangleCallsInExpr(*binary->left, modulePrefix, mangleThisModule);
        mangleCallsInExpr(*binary->right, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* unary = dynamic_cast<UnaryExpr*>(&expr)) {
        mangleCallsInExpr(*unary->operand, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* deref = dynamic_cast<DerefExpr*>(&expr)) {
        mangleCallsInExpr(*deref->pointer, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* indexExpr = dynamic_cast<IndexExpr*>(&expr)) {
        mangleCallsInExpr(*indexExpr->index, modulePrefix, mangleThisModule);
        return;
    }

    // IntegerExpr, VariableExpr, AddressOfExpr, FieldAccessExpr: no
    // nested Expr that could itself contain a call.
}

void mangleCallsInStatement(
    Statement& statement,
    const std::string& modulePrefix,
    bool mangleThisModule
) {
    if (auto* letStmt = dynamic_cast<LetStmt*>(&statement)) {
        mangleCallsInExpr(*letStmt->initializer, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* returnStmt = dynamic_cast<ReturnStmt*>(&statement)) {
        mangleCallsInExpr(*returnStmt->value, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* assignStmt = dynamic_cast<AssignStmt*>(&statement)) {
        mangleCallsInExpr(*assignStmt->value, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* storeStmt = dynamic_cast<StoreStmt*>(&statement)) {
        mangleCallsInExpr(*storeStmt->pointer, modulePrefix, mangleThisModule);
        mangleCallsInExpr(*storeStmt->value, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* indexStore = dynamic_cast<IndexStoreStmt*>(&statement)) {
        mangleCallsInExpr(*indexStore->index, modulePrefix, mangleThisModule);
        mangleCallsInExpr(*indexStore->value, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* fieldStore = dynamic_cast<FieldStoreStmt*>(&statement)) {
        mangleCallsInExpr(*fieldStore->value, modulePrefix, mangleThisModule);
        return;
    }

    if (auto* whileStmt = dynamic_cast<WhileStmt*>(&statement)) {
        mangleCallsInExpr(*whileStmt->condition, modulePrefix, mangleThisModule);
        for (auto& s : whileStmt->body) {
            mangleCallsInStatement(*s, modulePrefix, mangleThisModule);
        }
        return;
    }

    if (auto* ifStmt = dynamic_cast<IfStmt*>(&statement)) {
        mangleCallsInExpr(*ifStmt->condition, modulePrefix, mangleThisModule);
        for (auto& s : ifStmt->thenBody) {
            mangleCallsInStatement(*s, modulePrefix, mangleThisModule);
        }
        for (auto& s : ifStmt->elseBody) {
            mangleCallsInStatement(*s, modulePrefix, mangleThisModule);
        }
        return;
    }

    // ArrayDeclStmt, StructVarDeclStmt: no Expr to walk.
}

// A module's own struct/enum layouts, in the shape IR lowering needs
// (field order for structs, variant ordinals for enums) — built
// directly from the AST rather than from TypeChecker::ModuleSymbols,
// which uses unordered maps that don't preserve field declaration
// order.
struct ModuleLayouts {
    StructLayouts structLayouts;
    EnumOrdinals enumVariants;
};

ModuleLayouts collectLayouts(const Program& program) {
    ModuleLayouts layouts;

    for (const StructDecl& structDecl : program.structs) {
        std::vector<std::string> fieldNames;
        for (const Param& field : structDecl.fields) {
            fieldNames.push_back(field.name);
        }
        layouts.structLayouts[structDecl.name] = std::move(fieldNames);
    }

    for (const EnumDecl& enumDecl : program.enums) {
        std::unordered_map<std::string, int> variants;
        for (size_t i = 0; i < enumDecl.variants.size(); ++i) {
            variants[enumDecl.variants[i]] = static_cast<int>(i);
        }
        layouts.enumVariants[enumDecl.name] = std::move(variants);
    }

    return layouts;
}

std::string compileProgramToAssembly(
    const Program& program,
    const StructLayouts& structLayouts,
    const EnumOrdinals& enumVariants,
    X86Codegen& codegen
) {
    std::string assembly;

    for (const Function& function : program) {
        IRLowerer lowerer;
        IRFunction ir = lowerer.lower(function, structLayouts, enumVariants);

        LivenessAnalyzer liveness;
        auto ranges = liveness.analyze(ir);

        InterferenceAnalyzer interference;
        auto graph = interference.build(ranges);

        RegisterAllocator allocator;
        auto allocation = allocator.allocate(
            graph, ir.addressTakenValues, ir.arrayGroups
        );

        assembly += codegen.generate(ir, allocation, ranges);
    }

    return assembly;
}

int assembleLinkAndReport(const std::string& assembly) {
    std::ofstream output("main.s");

    if (!output) {
        std::cerr << "Cannot create main.s\n";
        return 1;
    }

    output << assembly;
    output.close();

    std::cout << "OGLang compilation successful.\n";
    std::cout << "Generated main.s\n";

    int asStatus = std::system("as --64 main.s -o main.o");
    if (asStatus != 0) {
        std::cerr << "Assembler failed\n";
        return 1;
    }

    int ldStatus = std::system("ld main.o -o main -e _start");
    if (ldStatus != 0) {
        std::cerr << "Linker failed\n";
        return 1;
    }

    std::cout << "Generated native executable: main\n";
    return 0;
}

int compileSingleFile(const std::string& sourcePath) {
    bool ok = false;
    std::string source = readFile(sourcePath, ok);

    if (!ok) {
        std::cerr << "Cannot open " << sourcePath << "\n";
        return 1;
    }

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Program program = parser.parseProgram();

    TypeChecker checker;
    checker.check(program);

    ModuleLayouts layouts = collectLayouts(program);

    X86Codegen codegen;
    std::string assembly = codegen.generateEntryPoint("main");
    assembly += compileProgramToAssembly(
        program, layouts.structLayouts, layouts.enumVariants, codegen
    );

    return assembleLinkAndReport(assembly);
}

// --------------------------------------------------------------
// Multi-file module compilation. See
// docs/ADR/0002-oglang-modules.md for the full design; the short
// version: one source file is one module, named after its filename
// (without extension); `import other;` makes `other`'s top-level
// functions/structs/enums reachable, but only as `other.symbol`
// (never unqualified) — which is what makes two modules sharing an
// unqualified name a non-issue. The first file on the command line is
// the program's entry module and is the only one required to declare
// `main`.
// --------------------------------------------------------------

struct ModuleUnit {
    std::string name;
    std::string path;
    Program program;
};

int compileModules(const std::vector<std::string>& sourcePaths) {
    std::vector<ModuleUnit> units;
    std::unordered_map<std::string, size_t> moduleIndex;

    for (const std::string& path : sourcePaths) {
        bool ok = false;
        std::string source = readFile(path, ok);

        if (!ok) {
            std::cerr << "Cannot open " << path << "\n";
            return 1;
        }

        std::string moduleName =
            std::filesystem::path(path).stem().string();

        if (moduleIndex.contains(moduleName)) {
            std::cerr << "Duplicate module name '" << moduleName
                      << "' (from " << path << " and "
                      << units[moduleIndex[moduleName]].path << ")\n";
            return 1;
        }

        Program program;
        try {
            Lexer lexer(source);
            auto tokens = lexer.tokenize();

            Parser parser(tokens);
            program = parser.parseProgram();
        } catch (const std::exception& e) {
            std::cerr << "Compilation error in '" << path << "': "
                      << e.what() << '\n';
            return 1;
        }

        moduleIndex[moduleName] = units.size();
        units.push_back({moduleName, path, std::move(program)});
    }

    // Resolve imports and build the dependency graph. adjacency[i]
    // lists the modules unit i imports (i depends on them).
    std::vector<std::vector<size_t>> adjacency(units.size());

    for (size_t i = 0; i < units.size(); ++i) {
        for (const ImportDecl& imp : units[i].program.imports) {
            auto it = moduleIndex.find(imp.moduleName);

            if (it == moduleIndex.end()) {
                std::cerr << "Unknown module in import: '"
                          << imp.moduleName << "' (imported by module '"
                          << units[i].name << "')\n";
                return 1;
            }

            if (it->second == i) {
                std::cerr << "Module '" << units[i].name
                          << "' imports itself\n";
                return 1;
            }

            adjacency[i].push_back(it->second);
        }
    }

    // Cycle detection: v1 supports no circular imports at all,
    // documented explicitly in ADR 0002 — a whole-program compiler
    // has no structural need for cycles (there is no separately
    // compiled object requiring a forward declaration across a
    // cycle), and rejecting them outright avoids having to define
    // what "compilation order" even means inside a cycle.
    std::vector<int> color(units.size(), 0);  // 0=white, 1=gray, 2=black
    std::vector<size_t> path;
    bool cycleFound = false;
    std::string cycleMessage;

    std::function<void(size_t)> dfs = [&](size_t node) {
        if (cycleFound) return;

        color[node] = 1;
        path.push_back(node);

        for (size_t next : adjacency[node]) {
            if (cycleFound) return;

            if (color[next] == 1) {
                std::string chain;
                bool started = false;
                for (size_t n : path) {
                    if (n == next) started = true;
                    if (started) {
                        if (!chain.empty()) chain += " -> ";
                        chain += units[n].name;
                    }
                }
                chain += " -> " + units[next].name;

                cycleMessage =
                    "Circular module import detected: " + chain;
                cycleFound = true;
                return;
            }

            if (color[next] == 0) {
                dfs(next);
            }
        }

        color[node] = 2;
        path.pop_back();
    };

    for (size_t i = 0; i < units.size() && !cycleFound; ++i) {
        if (color[i] == 0) {
            dfs(i);
        }
    }

    if (cycleFound) {
        std::cerr << cycleMessage << "\n";
        return 1;
    }

    // Deterministic compilation order: dependencies before
    // dependents (Kahn's algorithm), ties broken by picking the
    // lowest command-line index among the currently-ready modules —
    // this only affects diagnostic ordering today (the whole program
    // is checked/lowered as one unit either way), but keeps the
    // architecture ready for genuinely separate compilation later.
    std::vector<std::vector<size_t>> reverseAdjacency(units.size());
    std::vector<int> inDegree(units.size());

    for (size_t i = 0; i < units.size(); ++i) {
        inDegree[i] = static_cast<int>(adjacency[i].size());
        for (size_t dep : adjacency[i]) {
            reverseAdjacency[dep].push_back(i);
        }
    }

    std::vector<size_t> order;
    std::vector<bool> processed(units.size(), false);

    for (size_t count = 0; count < units.size(); ++count) {
        size_t chosen = units.size();

        for (size_t i = 0; i < units.size(); ++i) {
            if (!processed[i] && inDegree[i] == 0) {
                chosen = i;
                break;
            }
        }

        // Unreachable: the cycle check above already guarantees a
        // ready module always exists.
        order.push_back(chosen);
        processed[chosen] = true;

        for (size_t dependent : reverseAdjacency[chosen]) {
            inDegree[dependent]--;
        }
    }

    // Collect each module's own (unqualified) declarations. Building
    // this once up front, before any checking, is what makes
    // cross-module resolution independent of processing order beyond
    // needing dependencies collected before dependents check bodies —
    // which the topological order above already guarantees.
    std::vector<TypeChecker::ModuleSymbols> ownSymbols(units.size());

    for (size_t idx : order) {
        try {
            ownSymbols[idx] =
                TypeChecker::collectModuleSymbols(units[idx].program);
        } catch (const std::exception& e) {
            std::cerr << "Compilation error in module '"
                      << units[idx].name << "' (" << units[idx].path
                      << "): " << e.what() << '\n';
            return 1;
        }
    }

    // Type-check every module against its own declarations plus the
    // qualified declarations of whatever it imports. Only the entry
    // module (index 0 — the first file on the command line) is
    // required to declare `main`.
    for (size_t idx : order) {
        std::unordered_map<std::string, FunctionSignature> externalSignatures;
        StructTable externalStructs;
        EnumTable externalEnums;

        for (const ImportDecl& imp : units[idx].program.imports) {
            size_t depIndex = moduleIndex.at(imp.moduleName);
            const std::string& depName = units[depIndex].name;

            for (auto& [name, sig] : ownSymbols[depIndex].signatures) {
                externalSignatures[depName + "." + name] = sig;
            }
            for (auto& [name, fields] : ownSymbols[depIndex].structs) {
                externalStructs[depName + "." + name] = fields;
            }
            for (auto& [name, variants] : ownSymbols[depIndex].enums) {
                externalEnums[depName + "." + name] = variants;
            }
        }

        bool requireMain = (idx == 0);

        try {
            TypeChecker checker;
            checker.checkModule(
                units[idx].program,
                externalSignatures,
                externalStructs,
                externalEnums,
                requireMain
            );
        } catch (const std::exception& e) {
            std::cerr << "Compilation error in module '"
                      << units[idx].name << "' (" << units[idx].path
                      << "): " << e.what() << '\n';
            return 1;
        }
    }

    // Every module type-checked successfully. Mangle function
    // declarations/call sites for codegen (see mangleCallsInExpr's
    // comment) and lower the whole program — still one assembly file,
    // one assemble+link step, exactly like the single-file path; see
    // ADR 0002 for why this is honestly described as namespaced
    // whole-program compilation, not separately-compiled objects.
    for (size_t i = 0; i < units.size(); ++i) {
        bool isEntry = (i == 0);

        for (Function& function : units[i].program) {
            if (!isEntry) {
                function.name = units[i].name + "__" + function.name;
            }

            for (auto& statement : function.body) {
                mangleCallsInStatement(*statement, units[i].name, !isEntry);
            }
        }
    }

    X86Codegen codegen;
    std::string assembly = codegen.generateEntryPoint("main");

    for (size_t i = 0; i < units.size(); ++i) {
        ModuleLayouts layouts = collectLayouts(units[i].program);

        for (const ImportDecl& imp : units[i].program.imports) {
            size_t depIndex = moduleIndex.at(imp.moduleName);
            const std::string& depName = units[depIndex].name;

            ModuleLayouts depLayouts = collectLayouts(units[depIndex].program);

            for (auto& [name, fields] : depLayouts.structLayouts) {
                layouts.structLayouts[depName + "." + name] = fields;
            }
            for (auto& [name, variants] : depLayouts.enumVariants) {
                layouts.enumVariants[depName + "." + name] = variants;
            }
        }

        assembly += compileProgramToAssembly(
            units[i].program, layouts.structLayouts, layouts.enumVariants,
            codegen
        );
    }

    return assembleLinkAndReport(assembly);
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> sourcePaths;

    for (int i = 1; i < argc; ++i) {
        sourcePaths.push_back(argv[i]);
    }

    if (sourcePaths.empty()) {
        sourcePaths.push_back("main.og");
    }

    try {
        if (sourcePaths.size() == 1) {
            return compileSingleFile(sourcePaths[0]);
        }

        return compileModules(sourcePaths);

    } catch (const std::exception& e) {
        std::cerr << "Compilation error: "
                  << e.what() << '\n';
        return 1;
    }
}
