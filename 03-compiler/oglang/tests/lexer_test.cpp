#include "../lexer/lexer.hpp"
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

    for (const auto& token : tokens) {
        std::cout << token.lexeme << '\n';
    }
}
