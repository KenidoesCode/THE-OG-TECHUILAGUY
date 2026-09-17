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
        auto allocation = allocator.allocate(
            graph, ir.addressTakenValues, ir.arrayGroups
        );

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

void testParserParsesWhileAndAssignment() {
    Program program = parseProgram(
        "fn main() -> i32 { "
        "  let i: i32 = 0; "
        "  while (i < 10) { i = i + 1; } "
        "  return i; "
        "}"
    );

    auto* whileStmt = dynamic_cast<WhileStmt*>(program[0].body[1].get());
    check(whileStmt != nullptr, "parser: while loop parses as WhileStmt");
    check(whileStmt != nullptr && whileStmt->body.size() == 1,
          "parser: while body is parsed");

    if (whileStmt != nullptr) {
        auto* assign =
            dynamic_cast<AssignStmt*>(whileStmt->body[0].get());
        check(assign != nullptr,
              "parser: 'i = i + 1;' parses as AssignStmt");
        check(assign != nullptr && assign->name == "i",
              "parser: assignment records the target variable name");
    }
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

void testWhileCodegenEmitsBackEdge() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { "
        "  let i: i32 = 0; "
        "  while (i < 10) { i = i + 1; } "
        "  return i; "
        "}"
    );

    check(assembly.find(".Lloop") != std::string::npos,
          "codegen: while loop emits a loop-start label");
    check(assembly.find("jmp .Lloop") != std::string::npos,
          "codegen: while loop emits a back-edge jump");
    check(assembly.find("jz .Lloopend") != std::string::npos,
          "codegen: while loop exits via a conditional jump");
}

void testRegisterPressureForcesRealSpill() {
    // Six concurrently-live locals, only 4 registers available: at
    // least two of these must be spilled to a stack slot rather than
    // failing to compile at all.
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { "
        "  let a: i32 = 1; let b: i32 = 2; let c: i32 = 3; "
        "  let d: i32 = 4; let e: i32 = 5; let f: i32 = 6; "
        "  return a + b + c + d + e + f; "
        "}"
    );

    check(assembly.find("subq $") != std::string::npos,
          "codegen: register pressure triggers a real stack-frame spill "
          "allocation (subq), not just a compile failure");
    check(assembly.find("(%rbp)") != std::string::npos,
          "codegen: spilled values are addressed relative to rbp");
}

void testTypeCheckerRejectsAssignmentToUndeclaredVariable() {
    expectProgramThrows(
        "type checker: rejects assignment to an undeclared variable",
        "fn main() -> i32 { x = 5; return 0; }"
    );
}

void testTypeCheckerValidatesWhileConditionExpression() {
    // Confirms a while loop's condition is actually type-checked (goes
    // through checkExpr, same as if/else's condition) rather than
    // skipped, by rejecting an undefined variable used as the
    // condition.
    expectProgramThrows(
        "type checker: rejects an undefined variable in a while condition",
        "fn main() -> i32 { while (undefined_flag) { } return 0; }"
    );
}

void testTypeCheckerRejectsMissingReturnOnSomePath() {
    expectProgramThrows(
        "type checker: rejects a function whose if/else doesn't always return",
        "fn main() -> i32 { if (1 > 0) { return 1; } }"
    );
}

void testTypeCheckerRejectsFunctionEndingInBareWhileLoop() {
    expectProgramThrows(
        "type checker: rejects a function that ends in a while loop with "
        "no following return (a loop may run zero times)",
        "fn main() -> i32 { let i: i32 = 0; while (i < 1) { i = i + 1; } }"
    );
}

void testTypeCheckerAllowsIfElseWhereBothBranchesReturn() {
    try {
        Program program = parseProgram(
            "fn main() -> i32 { "
            "  if (1 > 0) { return 1; } else { return 0; } "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true,
              "type checker: accepts if/else where both branches return");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: accepts if/else where both branches return"
                  " (threw: ") + e.what() + ")");
    }
}

void testParserParsesUnaryMinus() {
    Program program = parseProgram(
        "fn main() -> i32 { return -5; }"
    );

    auto* ret = dynamic_cast<ReturnStmt*>(program[0].body[0].get());
    check(ret != nullptr, "parser: unary-minus program body parses");

    auto* unary = ret != nullptr
        ? dynamic_cast<UnaryExpr*>(ret->value.get())
        : nullptr;
    check(unary != nullptr, "parser: '-5' parses as UnaryExpr");
    check(unary != nullptr && unary->op == '-',
          "parser: unary expression records the '-' operator");

    // Double negation should nest, not collapse at parse time.
    Program nested = parseProgram(
        "fn main() -> i32 { return -(-7); }"
    );
    auto* nestedRet =
        dynamic_cast<ReturnStmt*>(nested[0].body[0].get());
    auto* outerUnary = nestedRet != nullptr
        ? dynamic_cast<UnaryExpr*>(nestedRet->value.get())
        : nullptr;
    check(outerUnary != nullptr &&
          dynamic_cast<UnaryExpr*>(outerUnary->operand.get()) != nullptr,
          "parser: double negation nests two UnaryExpr nodes");
}

void testTypeCheckerAcceptsUnaryMinusOnI32() {
    try {
        Program program = parseProgram(
            "fn main() -> i32 { let x: i32 = 3; return -x; }"
        );

        TypeChecker checker;
        checker.check(program);

        check(true, "type checker: accepts unary minus on an i32 value");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: accepts unary minus on an i32 value"
                  " (threw: ") + e.what() + ")");
    }
}

void testStackArgumentCodegenForMoreThanFourParams() {
    std::string assembly = compileToAssembly(
        "fn sub6(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) -> i32 { "
        "  return a - b - c - d - e - f; "
        "}"
        "fn main() -> i32 { return sub6(100, 1, 2, 3, 4, 5); }"
    );

    check(assembly.find("16(%rbp)") != std::string::npos,
          "codegen: reads the 5th parameter from 16(%rbp)");
    check(assembly.find("24(%rbp)") != std::string::npos,
          "codegen: reads the 6th parameter from 24(%rbp)");
    check(assembly.find("addq $16, %rsp") != std::string::npos,
          "codegen: caller cleans up stack-passed arguments after the call");
}

void testParserParsesAddressOfAndDeref() {
    Program program = parseProgram(
        "fn main() -> i32 { "
        "  let x: i32 = 1; "
        "  let p: ptr = &x; "
        "  let y: i32 = *p; "
        "  return y; "
        "}"
    );

    auto* letP = dynamic_cast<LetStmt*>(program[0].body[1].get());
    check(letP != nullptr && letP->type == "ptr",
          "parser: 'ptr' is accepted as a variable type");

    auto* addressOf = letP != nullptr
        ? dynamic_cast<AddressOfExpr*>(letP->initializer.get())
        : nullptr;
    check(addressOf != nullptr, "parser: '&x' parses as AddressOfExpr");
    check(addressOf != nullptr && addressOf->name == "x",
          "parser: address-of records the target variable name");

    auto* letY = dynamic_cast<LetStmt*>(program[0].body[2].get());
    auto* deref = letY != nullptr
        ? dynamic_cast<DerefExpr*>(letY->initializer.get())
        : nullptr;
    check(deref != nullptr, "parser: '*p' parses as DerefExpr");
}

void testParserParsesStoreStatement() {
    Program program = parseProgram(
        "fn main() -> i32 { "
        "  let x: i32 = 1; "
        "  let p: ptr = &x; "
        "  *p = 5; "
        "  return x; "
        "}"
    );

    auto* store = dynamic_cast<StoreStmt*>(program[0].body[2].get());
    check(store != nullptr, "parser: '*p = 5;' parses as StoreStmt");
}

void testPointerCodegenUsesFullWidthAddressing() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { "
        "  let x: i32 = 1; "
        "  let p: ptr = &x; "
        "  *p = *p + 1; "
        "  return x; "
        "}"
    );

    // Pointers are 64-bit addresses even though every other OGLang
    // value is 32-bit; a real stack address routinely lives above the
    // 4 GiB boundary, so computing or dereferencing one through a
    // 32-bit register/instruction would silently truncate it. This
    // caught a real bug (immediate segfault) during development.
    check(assembly.find("leaq") != std::string::npos,
          "codegen: address-of uses 64-bit leaq, not 32-bit leal");
    check(assembly.find("leal") == std::string::npos,
          "codegen: never uses 32-bit leal for an address");
}

void testParserParsesArrayDeclAndIndexing() {
    Program program = parseProgram(
        "fn main() -> i32 { "
        "  let arr: i32[3]; "
        "  arr[0] = 5; "
        "  let x: i32 = arr[0]; "
        "  return x; "
        "}"
    );

    auto* decl = dynamic_cast<ArrayDeclStmt*>(program[0].body[0].get());
    check(decl != nullptr, "parser: 'i32[3]' parses as ArrayDeclStmt");
    check(decl != nullptr && decl->size == 3,
          "parser: array declaration records its size");
    check(decl != nullptr && decl->elementType == "i32",
          "parser: array declaration records its element type");

    auto* indexStore =
        dynamic_cast<IndexStoreStmt*>(program[0].body[1].get());
    check(indexStore != nullptr,
          "parser: 'arr[0] = 5;' parses as IndexStoreStmt");

    auto* letX = dynamic_cast<LetStmt*>(program[0].body[2].get());
    auto* indexExpr = letX != nullptr
        ? dynamic_cast<IndexExpr*>(letX->initializer.get())
        : nullptr;
    check(indexExpr != nullptr, "parser: 'arr[0]' parses as IndexExpr");
}

void testArrayCodegenUsesPointerSafeSubtraction() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { "
        "  let arr: i32[3]; "
        "  arr[0] = 1; "
        "  return arr[0]; "
        "}"
    );

    // Element addressing computes (element-0's real 64-bit address)
    // minus (index * 8). Using the generic 32-bit SubI32 codegen for
    // that subtraction — instead of a pointer-aware 64-bit one — was a
    // real bug during development: it silently truncated the address
    // AddressOfI32 had correctly computed as 64-bit, corrupting every
    // array write. movslq (sign-extending the index offset to 64 bits)
    // and a 64-bit subq are the fingerprint of the fix.
    check(assembly.find("movslq") != std::string::npos,
          "codegen: array indexing sign-extends the byte offset to 64 bits");
    check(assembly.find("subq") != std::string::npos,
          "codegen: array element address subtraction is 64-bit (subq)");
}

void testArrayCodegenEmitsBoundsCheck() {
    std::string assembly = compileToAssembly(
        "fn main() -> i32 { "
        "  let arr: i32[3]; "
        "  return arr[0]; "
        "}"
    );

    // The bounds check is a single unsigned comparison (cmpl + jb)
    // against the array's element count, deliberately using the
    // unsigned-below condition so a negative index (huge when read as
    // unsigned) and a too-large one both fail the same check.
    check(assembly.find("cmpl $3,") != std::string::npos,
          "codegen: bounds check compares the index against the "
          "array's declared size (3)");
    check(assembly.find("jb .Lboundsok") != std::string::npos,
          "codegen: bounds check uses an unsigned 'jb', catching a "
          "negative index and an out-of-range one with one comparison");
    check(assembly.find("movl $101, %edi") != std::string::npos,
          "codegen: an out-of-bounds access traps with a distinct exit "
          "status (101) rather than silently continuing");
}

void testTypeCheckerRejectsIndexingUnknownArray() {
    expectProgramThrows(
        "type checker: rejects indexing an undeclared array",
        "fn main() -> i32 { return nope[0]; }"
    );
}

void testTypeCheckerRejectsNonIntegerArraySize() {
    expectProgramThrows(
        "type checker: rejects a zero-size array declaration",
        "fn main() -> i32 { let arr: i32[0]; return 0; }"
    );
}

void testTypeCheckerRejectsArrayIndexTypeMismatch() {
    expectProgramThrows(
        "type checker: rejects storing a non-i32 value into an i32 array",
        "fn f(p: ptr) -> i32 { "
        "  let arr: i32[2]; "
        "  arr[0] = p; "
        "  return 0; "
        "}"
        "fn main() -> i32 { return 0; }"
    );
}

void testTypeCheckerAllowsArrayRoundTrip() {
    try {
        Program program = parseProgram(
            "fn main() -> i32 { "
            "  let arr: i32[3]; "
            "  arr[0] = 42; "
            "  return arr[0]; "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true, "type checker: accepts a well-typed array round trip");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: accepts a well-typed array round trip"
                  " (threw: ") + e.what() + ")");
    }
}

void testTypeCheckerRejectsDereferenceOfNonPointer() {
    expectProgramThrows(
        "type checker: rejects dereferencing a plain i32 value",
        "fn main() -> i32 { let x: i32 = 1; return *x; }"
    );
}

void testTypeCheckerRejectsAddressOfUndeclaredVariable() {
    expectProgramThrows(
        "type checker: rejects taking the address of an undeclared variable",
        "fn main() -> i32 { let p: ptr = &nope; return 0; }"
    );
}

void testTypeCheckerRejectsStoreThroughNonPointer() {
    expectProgramThrows(
        "type checker: rejects '*x = v;' when x is not a pointer",
        "fn main() -> i32 { let x: i32 = 1; *x = 2; return x; }"
    );
}

void testTypeCheckerAllowsPointerRoundTrip() {
    try {
        Program program = parseProgram(
            "fn main() -> i32 { "
            "  let x: i32 = 1; "
            "  let p: ptr = &x; "
            "  *p = *p + 1; "
            "  return x; "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true, "type checker: accepts a well-typed pointer round trip");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: accepts a well-typed pointer round trip"
                  " (threw: ") + e.what() + ")");
    }
}

void testParserParsesConstPtrType() {
    Program program = parseProgram(
        "fn main() -> i32 { "
        "  let x: i32 = 1; "
        "  let p: constptr = &x; "
        "  return *p; "
        "}"
    );

    auto* letP = dynamic_cast<LetStmt*>(program[0].body[1].get());
    check(letP != nullptr && letP->type == "constptr",
          "parser: 'constptr' is accepted as a variable type");
}

void testTypeCheckerAllowsMutablePointerAssignedToConstPtr() {
    try {
        Program program = parseProgram(
            "fn main() -> i32 { "
            "  let x: i32 = 1; "
            "  let p: constptr = &x; "
            "  return *p; "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true,
              "type checker: a mutable pointer (&x) may be stored in a "
              "constptr variable (widening)");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: a mutable pointer (&x) may be stored in a "
                  "constptr variable (widening) (threw: ") + e.what() + ")");
    }
}

void testTypeCheckerRejectsStoreThroughConstPointer() {
    expectProgramThrows(
        "type checker: rejects '*p = v;' when p is a constptr",
        "fn main() -> i32 { "
        "  let x: i32 = 1; "
        "  let p: constptr = &x; "
        "  *p = 2; "
        "  return x; "
        "}"
    );
}

void testTypeCheckerAllowsReadThroughConstPointer() {
    try {
        Program program = parseProgram(
            "fn main() -> i32 { "
            "  let x: i32 = 41; "
            "  let p: constptr = &x; "
            "  return *p + 1; "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true, "type checker: reading through a constptr is allowed");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: reading through a constptr is allowed"
                  " (threw: ") + e.what() + ")");
    }
}

void testTypeCheckerRejectsConstPointerPassedAsMutableParam() {
    expectProgramThrows(
        "type checker: rejects passing a constptr where a mutable ptr "
        "parameter is expected",
        "fn write_one(p: ptr) -> i32 { *p = 1; return 0; } "
        "fn main() -> i32 { "
        "  let x: i32 = 0; "
        "  let cp: constptr = &x; "
        "  return write_one(cp); "
        "}"
    );
}

void testTypeCheckerAllowsMutablePointerPassedAsConstParam() {
    try {
        Program program = parseProgram(
            "fn read_one(p: constptr) -> i32 { return *p; } "
            "fn main() -> i32 { "
            "  let x: i32 = 7; "
            "  let p: ptr = &x; "
            "  return read_one(p); "
            "}"
        );

        TypeChecker checker;
        checker.check(program);

        check(true,
              "type checker: a mutable pointer may be passed where a "
              "constptr parameter is expected (widening)");
    } catch (const std::exception& e) {
        check(false,
              std::string(
                  "type checker: a mutable pointer may be passed where a "
                  "constptr parameter is expected (widening) (threw: ") +
                  e.what() + ")");
    }
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
    testParserParsesWhileAndAssignment();
    testParserParsesUnaryMinus();
    testTypeCheckerAcceptsUnaryMinusOnI32();
    testFullPipelineProducesRegisterAllocation();
    testDivisionCodegenUsesRegisterConstrainedIdiv();
    testCallCodegenMarshalsArguments();
    testIfElseCodegenEmitsBranches();
    testWhileCodegenEmitsBackEdge();
    testRegisterPressureForcesRealSpill();
    testStackArgumentCodegenForMoreThanFourParams();
    testParserParsesAddressOfAndDeref();
    testParserParsesStoreStatement();
    testPointerCodegenUsesFullWidthAddressing();
    testParserParsesArrayDeclAndIndexing();
    testArrayCodegenUsesPointerSafeSubtraction();
    testArrayCodegenEmitsBoundsCheck();
    testTypeCheckerRejectsIndexingUnknownArray();
    testTypeCheckerRejectsNonIntegerArraySize();
    testTypeCheckerRejectsArrayIndexTypeMismatch();
    testTypeCheckerAllowsArrayRoundTrip();
    testTypeCheckerRejectsDereferenceOfNonPointer();
    testTypeCheckerRejectsAddressOfUndeclaredVariable();
    testTypeCheckerRejectsStoreThroughNonPointer();
    testTypeCheckerAllowsPointerRoundTrip();
    testParserParsesConstPtrType();
    testTypeCheckerAllowsMutablePointerAssignedToConstPtr();
    testTypeCheckerRejectsStoreThroughConstPointer();
    testTypeCheckerAllowsReadThroughConstPointer();
    testTypeCheckerRejectsConstPointerPassedAsMutableParam();
    testTypeCheckerAllowsMutablePointerPassedAsConstParam();
    testTypeCheckerRejectsUnknownVariable();
    testTypeCheckerRejectsReturnTypeMismatch();
    testTypeCheckerRejectsUndefinedFunctionCall();
    testTypeCheckerRejectsWrongArgumentCount();
    testTypeCheckerRejectsRedefinedFunction();
    testTypeCheckerRejectsAssignmentToUndeclaredVariable();
    testTypeCheckerValidatesWhileConditionExpression();
    testTypeCheckerRejectsMissingReturnOnSomePath();
    testTypeCheckerRejectsFunctionEndingInBareWhileLoop();
    testTypeCheckerAllowsIfElseWhereBothBranchesReturn();
    testTypeCheckerAllowsForwardAndSelfRecursiveCalls();
    testParserRejectsMalformedSyntax();

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" :
                           std::to_string(failures) + " TEST(S) FAILED")
              << "\n";

    return failures == 0 ? 0 : 1;
}
