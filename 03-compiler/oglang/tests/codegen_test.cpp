#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../semantic/analyzer.hpp"
#include "../ir/lower.hpp"
#include "../codegen/x86_64.hpp"

#include <iostream>

int main() {
    std::string source =
        "fn main() -> i32 { return 42; }";

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Function function = parser.parseFunction();

    SemanticAnalyzer analyzer;
    analyzer.analyze(function);

    IRLowerer lowerer;
    IRFunction ir = lowerer.lower(function);

    X86Codegen codegen;
    std::cout << codegen.generate(ir);
}
