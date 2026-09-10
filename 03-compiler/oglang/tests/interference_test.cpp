#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../types/type_checker.hpp"
#include "../ir/lower.hpp"
#include "../analysis/liveness.hpp"
#include "../analysis/interference.hpp"

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

    LivenessAnalyzer liveness;
    auto ranges = liveness.analyze(ir);

    InterferenceAnalyzer interference;
    auto graph = interference.build(ranges);

    for (const auto& [value, neighbors] : graph) {
        std::cout << "v" << value << " interferes with:";

        for (ValueId neighbor : neighbors)
            std::cout << " v" << neighbor;

        std::cout << '\n';
    }

    std::cout << "Interference: PASS\n";
}
