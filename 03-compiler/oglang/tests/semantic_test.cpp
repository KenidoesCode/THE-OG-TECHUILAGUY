#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../semantic/analyzer.hpp"
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

    std::cout << "Semantic analysis: PASS\n";
}
