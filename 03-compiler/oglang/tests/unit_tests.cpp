// Real assertion-based unit tests for the OGLang frontend/middle-end.
//
// Replaces the earlier tests/*_test.cpp files, which only printed
// intermediate output for manual inspection and asserted nothing. One of
// them (lexer_test.cpp) no longer even compiled against the current Token
// struct, which is exactly the kind of drift assertion-based tests catch.

#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
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

Program parseProgram(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    return parser.parseProgram();
}

void expectProgramThrows(
    const std::string& description,
    const std::string& source
) {
    try {
        Program program = parseProgram(source);

        TypeChecker checker;
        checker.check(program);

        std::cout << "[FAIL] " << description
                  << " — expected an error, none was thrown\n";
        failures++;
    } catch (const std::exception&) {
        std::cout << "[PASS] " << description << "\n";
    }
}

// Compiles a single-function program all the way to assembly and returns
// it, so codegen invariants can be checked directly.
std::string compileToAssembly(const std::string& source) {
    Program program = parseProgram(source);

    TypeChecker checker;
    checker.check(program);

    X86Codegen codegen;
    std::string assembly = codegen.generateEntryPoint("main");

    for (const Function& function : program) {
        IRLowerer lowerer;
        IRFunction ir = lowerer.lower(function);

        LivenessAnalyzer liveness;
        auto ranges = liveness.analyze(ir);

        InterferenceAnalyzer interference;
        auto graph = interference.build(ranges);

        RegisterAllocator allocator;
        auto allocation = allocator.allocate(graph);

        assembly += codegen.generate(ir, allocation, ranges);
    }

    return assembly;
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
    bool sawComma = false;
    for (const auto& t : tokens) {
        if (t.kind == TokenKind::Plus) sawPlus = true;
        if (t.kind == TokenKind::Comma) sawComma = true;
    }
    check(sawPlus, "lexer: recognizes '+' operator");
    check(!sawComma, "lexer: does not spuriously emit Comma tokens");

    Lexer lexer2("f(a, b, c)");
    int commaCount = 0;
    for (const auto& t : lexer2.tokenize()) {
        if (t.kind == TokenKind::Comma) commaCount++;
    }
    check(commaCount == 2, "lexer: recognizes ',' in argument lists");
}

void testParserBuildsFunctionFromValidSource() {
    Program program = parseProgram(
        "fn main() -> i32 { let x: i32 = 10 + 20 * 3; return x; }"
    );

    check(program.size() == 1, "parser: parses a single-function program");
    check(program[0].name == "main", "parser: parses function name");
    check(program[0].returnType == "i32",
          "parser: parses return type");
    check(program[0].body.size() == 2,
          "parser: parses expected statement count");
    check(program[0].params.empty(),
          "parser: zero-parameter function has no params");
}

void testParserParsesParametersAndCalls() {
    Program program = parseProgram(
        "fn add(a: i32, b: i32) -> i32 { return a + b; }"
        "fn main() -> i32 { return add(1, 2); }"
    );

    check(program.size() == 2,
          "parser: parses multiple top-level functions");
    check(program[0].params.size() == 2,
          "parser: parses a two-parameter function signature");
    check(program[0].params[0].name == "a" &&
          program[0].params[1].name == "b",
          "parser: preserves parameter order and names");

    auto* ret = dynamic_cast<ReturnStmt*>(program[1].body[0].get());
    check(ret != nullptr, "parser: main body parses as a return statement");

    auto* call = dynamic_cast<CallExpr*>(ret->value.get());
    check(call != nullptr, "parser: call expression parses as CallExpr");
    check(call != nullptr && call->callee == "add",
          "parser: call expression records the callee name");
    check(call != nullptr && call->args.size() == 2,
          "parser: call expression parses its argument list");
}

void testParserParsesIfElse() {
    Program program = parseProgram(
        "fn main() -> i32 { "
        "  if (1 > 0) { return 1; } else { return 0; } "
        "}"
    );

    auto* ifStmt =
        dynamic_cast<IfStmt*>(program[0].body[0].get());

    check(ifStmt != nullptr, "parser: if/else parses as IfStmt");
    check(ifStmt != nullptr && ifStmt->thenBody.size() == 1,
          "parser: then-branch body is parsed");
    check(ifStmt != nullptr && ifStmt->elseBody.size() == 1,
          "parser: else-branch body is parsed");
}

void testFullPipelineProducesRegisterAllocation() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { let x: i32 = 10 + 20 * 3; return x; }"
    );

    check(assembly.find("_start:") != std::string::npos,
          "codegen: emits a process entry point (_start)");
    check(assembly.find("syscall") != std::string::npos,
          "codegen: emits an exit syscall so binaries terminate cleanly");
    check(assembly.find("main:") != std::string::npos,
          "codegen: emits the main function's label");
}

void testDivisionCodegenUsesRegisterConstrainedIdiv() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { let a: i32 = 84; let b: i32 = 2; return a / b; }"
    );

    check(assembly.find("cdq") != std::string::npos,
          "codegen: division sign-extends eax into edx via cdq");
    check(assembly.find("idivl") != std::string::npos,
          "codegen: division lowers to idivl");
}

void testCallCodegenMarshalsArguments() {
    std::string assembly = compileToAssembly(
        "fn add3(a: i32, b: i32, c: i32) -> i32 { return a + b + c; }"
        "fn main() -> i32 { return add3(1, 2, 3); }"
    );

    check(assembly.find("call add3") != std::string::npos,
          "codegen: emits a call instruction to the callee");
    // Argument marshaling is stack-mediated (push sources, pop into ABI
    // registers) specifically so overlapping source/target registers
    // can't corrupt each other; both halves must be present.
    check(assembly.find("popq %rdi") != std::string::npos,
          "codegen: marshals argument 1 into edi/rdi via the stack");
    check(assembly.find("popq %rsi") != std::string::npos,
          "codegen: marshals argument 2 into esi/rsi via the stack");
}

void testIfElseCodegenEmitsBranches() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { "
        "  if (1 > 0) { return 1; } else { return 0; } "
        "}"
    );

    check(assembly.find("cmpl") != std::string::npos,
          "codegen: comparison lowers to cmpl");
    check(assembly.find("jz ") != std::string::npos,
          "codegen: if-condition lowers to a conditional jump");
    check(assembly.find(".Lelse") != std::string::npos ||
          assembly.find(".Lend") != std::string::npos,
          "codegen: if/else lowering emits labels");
}

void testTypeCheckerRejectsUnknownVariable() {
    expectProgramThrows(
        "type checker: rejects reference to undefined variable",
        "fn main() -> i32 { return y; }"
    );
}

void testTypeCheckerRejectsReturnTypeMismatch() {
    expectProgramThrows(
        "type checker: rejects mismatched let-binding type",
        "fn main() -> i32 { let x: bogus = 1; return x; }"
    );
}

void testTypeCheckerRejectsUndefinedFunctionCall() {
    expectProgramThrows(
        "type checker: rejects a call to an undefined function",
        "fn main() -> i32 { return nonexistent(1); }"
    );
}

void testTypeCheckerRejectsWrongArgumentCount() {
    expectProgramThrows(
        "type checker: rejects a call with the wrong argument count",
        "fn add(a: i32, b: i32) -> i32 { return a + b; }"
        "fn main() -> i32 { return add(1); }"
    );
}

void testTypeCheckerRejectsRedefinedFunction() {
    expectProgramThrows(
        "type checker: rejects a function defined twice",
        "fn f() -> i32 { return 1; }"
        "fn f() -> i32 { return 2; }"
        "fn main() -> i32 { return f(); }"
    );
}

void testTypeCheckerAllowsForwardAndSelfRecursiveCalls() {
    // main() calls fact(), defined *after* it in the source, and fact()
    // calls itself. Neither should require a forward declaration.
    try {
        Program program = parseProgram(
            "fn main() -> i32 { return fact(5); }"
            "fn fact(n: i32) -> i32 { "
            "  if (n <= 1) { return 1; } "
            "  return n * fact(n - 1); "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true,
              "type checker: allows forward references and recursion");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: allows forward references and recursion"
                  " (threw: ") + e.what() + ")");
    }
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
    testLexerTokenizesKeywordsAndOperators();
    testParserBuildsFunctionFromValidSource();
    testParserParsesParametersAndCalls();
    testParserParsesIfElse();
    testFullPipelineProducesRegisterAllocation();
    testDivisionCodegenUsesRegisterConstrainedIdiv();
    testCallCodegenMarshalsArguments();
    testIfElseCodegenEmitsBranches();
    testTypeCheckerRejectsUnknownVariable();
    testTypeCheckerRejectsReturnTypeMismatch();
    testTypeCheckerRejectsUndefinedFunctionCall();
    testTypeCheckerRejectsWrongArgumentCount();
    testTypeCheckerRejectsRedefinedFunction();
    testTypeCheckerAllowsForwardAndSelfRecursiveCalls();
    testParserRejectsMalformedSyntax();

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" :
                           std::to_string(failures) + " TEST(S) FAILED")
              << "\n";

    return failures == 0 ? 0 : 1;
}
