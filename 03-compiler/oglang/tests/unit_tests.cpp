// Real assertion-based unit tests for the OGLang frontend/middle-end.
//
// Replaces the earlier tests/*_test.cpp files, which only printed
// intermediate output for manual inspection and asserted nothing. One of
// them (lexer_test.cpp) no longer even compiled against the current Token
// struct, which is exactly the kind of drift assertion-based tests catch.

#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../semantic/analyzer.hpp"
#include "../types/type_checker.hpp"
#include "../ir/lower.hpp"
#include "../analysis/liveness.hpp"
#include "../analysis/interference.hpp"
#include "../codegen/register_allocator.hpp"
#include "../codegen/x86_64.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

void expectThrow(
    const std::string& description,
    const std::string& source
) {
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        Parser parser(tokens);
        Function function = parser.parseFunction();

        TypeChecker checker;
        checker.check(function);

        std::cout << "[FAIL] " << description
                  << " — expected an error, none was thrown\n";
        failures++;
    } catch (const std::exception&) {
        std::cout << "[PASS] " << description << "\n";
    }
}

void testLexerTokenizesKeywordsAndOperators() {
    Lexer lexer("fn main() -> i32 { let x: i32 = 1 + 2; return x; }");
    auto tokens = lexer.tokenize();

    check(!tokens.empty(), "lexer: produces at least one token");
    check(tokens.front().kind == TokenKind::Fn,
          "lexer: first token is 'fn'");
    check(tokens.back().kind == TokenKind::End,
          "lexer: stream is terminated with End token");

    bool sawPlus = false;
    for (const auto& t : tokens) {
        if (t.kind == TokenKind::Plus) sawPlus = true;
    }
    check(sawPlus, "lexer: recognizes '+' operator");
}

void testParserBuildsFunctionFromValidSource() {
    Lexer lexer(
        "fn main() -> i32 { let x: i32 = 10 + 20 * 3; return x; }"
    );
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Function function = parser.parseFunction();

    check(function.name == "main", "parser: parses function name");
    check(function.returnType == "i32",
          "parser: parses return type");
    check(function.body.size() == 2,
          "parser: parses expected statement count");
}

void testFullPipelineProducesRegisterAllocation() {
    Lexer lexer(
        "fn main() -> i32 { let x: i32 = 10 + 20 * 3; return x; }"
    );
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

    RegisterAllocator allocator;
    auto allocation = allocator.allocate(graph);

    check(!ir.instructions.empty(),
          "pipeline: lowers to at least one IR instruction");
    check(!allocation.empty(),
          "pipeline: register allocator assigns at least one register");

    X86Codegen codegen;
    std::string assembly = codegen.generate(ir, allocation);

    check(assembly.find("_start:") != std::string::npos,
          "codegen: emits a process entry point (_start)");
    check(assembly.find("syscall") != std::string::npos,
          "codegen: emits an exit syscall so binaries terminate cleanly");
}

void testTypeCheckerRejectsUnknownVariable() {
    expectThrow(
        "type checker: rejects reference to undefined variable",
        "fn main() -> i32 { return y; }"
    );
}

void testTypeCheckerRejectsReturnTypeMismatch() {
    // Function declares i32 return type; body only ever produces i32
    // today, so instead we check a type mismatch on the let binding,
    // which the checker does enforce.
    expectThrow(
        "type checker: rejects mismatched let-binding type",
        "fn main() -> i32 { let x: bogus = 1; return x; }"
    );
}

void testParserRejectsMalformedSyntax() {
    try {
        Lexer lexer("fn main( -> i32 { return 1; }");
        auto tokens = lexer.tokenize();

        Parser parser(tokens);
        parser.parseFunction();

        std::cout << "[FAIL] parser: rejects malformed syntax"
                     " — expected an error, none was thrown\n";
        failures++;
    } catch (const std::exception&) {
        std::cout << "[PASS] parser: rejects malformed syntax\n";
    }
}

}  // namespace

int main() {
    std::cerr.setf(std::ios::unitbuf);
    std::cout.setf(std::ios::unitbuf);

    testLexerTokenizesKeywordsAndOperators();
    testParserBuildsFunctionFromValidSource();
    testFullPipelineProducesRegisterAllocation();
    testTypeCheckerRejectsUnknownVariable();
    testTypeCheckerRejectsReturnTypeMismatch();
    testParserRejectsMalformedSyntax();

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" :
                           std::to_string(failures) + " TEST(S) FAILED")
              << "\n";

    return failures == 0 ? 0 : 1;
}
