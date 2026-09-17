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

        X86Codegen codegen;
        std::string assembly = codegen.generateEntryPoint("main");

        for (const Function& function : program) {
            IRLowerer lowerer;
            IRFunction ir = lowerer.lower(function);

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
