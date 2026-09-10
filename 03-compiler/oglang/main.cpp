#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/analyzer.hpp"
#include "ir/lower.hpp"
#include "codegen/x86_64.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

int main() {
    std::ifstream file("main.og");

    if (!file) {
        std::cerr << "Cannot open main.og\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    Lexer lexer(buffer.str());
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Function function = parser.parseFunction();

    SemanticAnalyzer analyzer;
    analyzer.analyze(function);

    IRLowerer lowerer;
    IRFunction ir = lowerer.lower(function);

    X86Codegen codegen;
    std::string assembly = codegen.generate(ir);

    std::ofstream output("main.s");
    output << assembly;

    std::cout << "Compiled main.og -> main.s\n";
}
