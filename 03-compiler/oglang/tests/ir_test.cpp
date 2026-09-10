#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../types/type_checker.hpp"
#include "../ir/lower.hpp"

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

    for (const auto& instruction : ir.instructions) {
        switch (instruction.opcode) {
            case OpCode::ConstI32:
                std::cout << "CONST "
                          << instruction.value
                          << " -> "
                          << instruction.destination
                          << '\n';
                break;

            case OpCode::AddI32:
                std::cout << "ADD "
                          << instruction.left
                          << ", "
                          << instruction.right
                          << " -> "
                          << instruction.destination
                          << '\n';
                break;

            case OpCode::ReturnI32:
                std::cout << "RETURN "
                          << instruction.left
                          << '\n';
                break;
        }
    }

    std::cout << "IR: PASS\n";
}
