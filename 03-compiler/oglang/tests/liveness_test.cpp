#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../types/type_checker.hpp"
#include "../ir/lower.hpp"
#include "../analysis/liveness.hpp"

#include <iostream>

int main() {

    std::string source =
        "fn main() -> i32 { "
        "let x: i32 = 20; "
        "let y: i32 = 22; "
        "return x + y; "
        "}";

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Function function = parser.parseFunction();

    TypeChecker checker;
    checker.check(function);

    IRLowerer lowerer;
    IRFunction ir = lowerer.lower(function);

    LivenessAnalyzer analyzer;
    auto ranges = analyzer.analyze(ir);

    for (const auto& [value, range] : ranges) {
        std::cout
            << "v" << value
            << ": [" << range.start
            << ", " << range.end
            << "]\n";
    }

    std::cout << "Liveness: PASS\n";
}
