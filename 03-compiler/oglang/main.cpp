#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/type_checker.hpp"
#include "ir/lower.hpp"
#include "analysis/liveness.hpp"
#include "analysis/interference.hpp"
#include "codegen/register_allocator.hpp"
#include "codegen/x86_64.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

int main(int argc, char** argv) {
    std::string sourcePath = argc > 1 ? argv[1] : "main.og";

    std::ifstream file(sourcePath);

    if (!file) {
        std::cerr << "Cannot open " << sourcePath << "\n";
        return 1;
    }

    std::string source(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        Parser parser(tokens);
        Program program = parser.parseProgram();

        TypeChecker checker;
        checker.check(program);

        std::unordered_map<std::string, std::vector<std::string>>
            structLayouts;
        for (const StructDecl& structDecl : program.structs) {
            std::vector<std::string> fieldNames;
            for (const Param& field : structDecl.fields) {
                fieldNames.push_back(field.name);
            }
            structLayouts[structDecl.name] = std::move(fieldNames);
        }

        std::unordered_map<std::string, std::unordered_map<std::string, int>>
            enumVariants;
        for (const EnumDecl& enumDecl : program.enums) {
            std::unordered_map<std::string, int> variants;
            for (size_t i = 0; i < enumDecl.variants.size(); ++i) {
                variants[enumDecl.variants[i]] = static_cast<int>(i);
            }
            enumVariants[enumDecl.name] = std::move(variants);
        }

        X86Codegen codegen;
        std::string assembly = codegen.generateEntryPoint("main");

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

    } catch (const std::exception& e) {
        std::cerr << "Compilation error: "
                  << e.what() << '\n';
        return 1;
    }

    return 0;
}
