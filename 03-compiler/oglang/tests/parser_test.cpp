#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
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

    std::cout << "Function: " << function.name << '\n';
    std::cout << "Statements: " << function.body.size() << '\n';

    auto* x = dynamic_cast<LetStmt*>(function.body[0].get());
    auto* y = dynamic_cast<LetStmt*>(function.body[1].get());
    auto* ret = dynamic_cast<ReturnStmt*>(function.body[2].get());

    auto* sum = dynamic_cast<BinaryExpr*>(ret->value.get());

    std::cout << "Variable 1: " << x->name << '\n';
    std::cout << "Variable 2: " << y->name << '\n';
    std::cout << "Operator: " << sum->op << '\n';

    auto* left = dynamic_cast<VariableExpr*>(sum->left.get());
    auto* right = dynamic_cast<VariableExpr*>(sum->right.get());

    std::cout << "Left: " << left->name << '\n';
    std::cout << "Right: " << right->name << '\n';
}
