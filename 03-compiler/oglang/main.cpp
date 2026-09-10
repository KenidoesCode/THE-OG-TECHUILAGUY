#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/type_checker.hpp"
#include "ir/lower.hpp"
#include "codegen/x86_64.hpp"

#include <fstream>
#include <iostream>

int main() {
    std::ifstream file("main.og");

    if (!file) {
        std::cerr << "Cannot open main.og\n";
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
        Function function = parser.parseFunction();

        TypeChecker checker;
        checker.check(function);

        IRLowerer lowerer;
        IRFunction ir = lowerer.lower(function);

        X86Codegen codegen;
        std::string assembly = codegen.generate(ir);

        std::ofstream output("main.s");
        output << assembly;

        std::cout << "OGLang compilation successful.\n";
        std::cout << "Generated main.s\n";

    } catch (const std::exception& e) {
        std::cerr << "Compilation error: "
                  << e.what() << '\n';
        return 1;
    }

    return 0;
}
