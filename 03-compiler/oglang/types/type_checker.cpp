#include "type_checker.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class Checker {
public:
    explicit Checker(
        const std::unordered_map<std::string, FunctionSignature>& signatures
    )
        : signatures(signatures) {}

    std::unordered_map<std::string, std::string> variables;

    // name -> (element type, size). A separate namespace from
    // `variables`: an array is never used as a plain value, only
    // indexed, so there is no ambiguity in keeping them apart.
    std::unordered_map<std::string, std::pair<std::string, int>> arrays;

    std::string checkExpr(const Expr& expr) {
        if (dynamic_cast<const IntegerExpr*>(&expr))
            return "i32";

        if (auto* variable =
                dynamic_cast<const VariableExpr*>(&expr)) {

            auto it = variables.find(variable->name);

            if (it == variables.end()) {
                throw std::runtime_error(
                    "Unknown variable: " + variable->name
                );
            }

            return it->second;
        }

        if (auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
            std::string operandType = checkExpr(*unary->operand);

            if (operandType != "i32") {
                throw std::runtime_error(
                    "Unary operator requires an i32 operand"
                );
            }

            return "i32";
        }

        if (auto* addressOf =
                dynamic_cast<const AddressOfExpr*>(&expr)) {

            if (!variables.contains(addressOf->name)) {
                throw std::runtime_error(
                    "Cannot take the address of undeclared variable: " +
                    addressOf->name
                );
            }

            return "ptr";
        }

        if (auto* deref = dynamic_cast<const DerefExpr*>(&expr)) {
            std::string pointerType = checkExpr(*deref->pointer);

            if (pointerType != "ptr") {
                throw std::runtime_error(
                    "Cannot dereference a non-pointer value"
                );
            }

            return "i32";
        }

        if (auto* indexExpr = dynamic_cast<const IndexExpr*>(&expr)) {
            auto it = arrays.find(indexExpr->arrayName);

            if (it == arrays.end()) {
                throw std::runtime_error(
                    "Indexing unknown array: " + indexExpr->arrayName
                );
            }

            std::string indexType = checkExpr(*indexExpr->index);

            if (indexType != "i32") {
                throw std::runtime_error(
                    "Array index must be i32"
                );
            }

            return it->second.first;
        }

        if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
            auto it = signatures.find(call->callee);

            if (it == signatures.end()) {
                throw std::runtime_error(
                    "Call to undefined function: " + call->callee
                );
            }

            const FunctionSignature& sig = it->second;

            if (call->args.size() != sig.paramTypes.size()) {
                throw std::runtime_error(
                    "Function '" + call->callee + "' expects " +
                    std::to_string(sig.paramTypes.size()) +
                    " argument(s), got " +
                    std::to_string(call->args.size())
                );
            }

            for (size_t i = 0; i < call->args.size(); ++i) {
                std::string argType = checkExpr(*call->args[i]);

                if (argType != sig.paramTypes[i]) {
                    throw std::runtime_error(
                        "Argument " + std::to_string(i + 1) +
                        " to '" + call->callee +
                        "' has wrong type"
                    );
                }
            }

            return sig.returnType;
        }

        if (auto* binary =
                dynamic_cast<const BinaryExpr*>(&expr)) {

            std::string left = checkExpr(*binary->left);
            std::string right = checkExpr(*binary->right);

            if (left != "i32" || right != "i32") {
                throw std::runtime_error(
                    "Binary operator requires i32 operands"
                );
            }

            switch (binary->op) {
                case '+':
                case '-':
                case '*':
                case '/':
                case '=':
                case '!':
                case '>':
                case '<':
                case 'G':
                case 'L':
                    // Comparisons also produce i32 (0 or 1); OGLang
                    // has no distinct boolean type yet.
                    return "i32";

                default:
                    throw std::runtime_error(
                        "Unknown binary operator"
                    );
            }
        }

        throw std::runtime_error("Unknown expression");
    }

    void checkBlock(
        const std::vector<std::unique_ptr<Statement>>& body,
        const std::string& functionReturnType
    ) {
        for (const auto& statement : body) {
            checkStatement(*statement, functionReturnType);
        }
    }

    void checkStatement(
        const Statement& statement,
        const std::string& functionReturnType
    ) {
        if (auto* letStmt =
                dynamic_cast<const LetStmt*>(&statement)) {

            std::string actual = checkExpr(*letStmt->initializer);

            if (actual != letStmt->type) {
                throw std::runtime_error(
                    "Type mismatch for variable: " + letStmt->name
                );
            }

            variables[letStmt->name] = letStmt->type;
            return;
        }

        if (auto* assignStmt =
                dynamic_cast<const AssignStmt*>(&statement)) {

            auto it = variables.find(assignStmt->name);

            if (it == variables.end()) {
                throw std::runtime_error(
                    "Assignment to undeclared variable: " +
                    assignStmt->name
                );
            }

            std::string actual = checkExpr(*assignStmt->value);

            if (actual != it->second) {
                throw std::runtime_error(
                    "Type mismatch assigning to variable: " +
                    assignStmt->name
                );
            }

            return;
        }

        if (auto* storeStmt =
                dynamic_cast<const StoreStmt*>(&statement)) {

            std::string pointerType = checkExpr(*storeStmt->pointer);

            if (pointerType != "ptr") {
                throw std::runtime_error(
                    "Cannot store through a non-pointer value"
                );
            }

            std::string valueType = checkExpr(*storeStmt->value);

            if (valueType != "i32") {
                throw std::runtime_error(
                    "Cannot store a non-i32 value through a pointer"
                );
            }

            return;
        }

        if (auto* arrayDecl =
                dynamic_cast<const ArrayDeclStmt*>(&statement)) {

            if (arrayDecl->elementType != "i32") {
                throw std::runtime_error(
                    "Only i32 arrays are supported"
                );
            }

            if (arrayDecl->size <= 0) {
                throw std::runtime_error(
                    "Array size must be positive: " + arrayDecl->name
                );
            }

            arrays[arrayDecl->name] =
                {arrayDecl->elementType, arrayDecl->size};

            return;
        }

        if (auto* indexStore =
                dynamic_cast<const IndexStoreStmt*>(&statement)) {

            auto it = arrays.find(indexStore->arrayName);

            if (it == arrays.end()) {
                throw std::runtime_error(
                    "Indexing unknown array: " + indexStore->arrayName
                );
            }

            std::string indexType = checkExpr(*indexStore->index);

            if (indexType != "i32") {
                throw std::runtime_error(
                    "Array index must be i32"
                );
            }

            std::string valueType = checkExpr(*indexStore->value);

            if (valueType != it->second.first) {
                throw std::runtime_error(
                    "Type mismatch storing into array: " +
                    indexStore->arrayName
                );
            }

            return;
        }

        if (auto* whileStmt =
                dynamic_cast<const WhileStmt*>(&statement)) {

            std::string condType = checkExpr(*whileStmt->condition);

            if (condType != "i32") {
                throw std::runtime_error(
                    "While condition must be i32 (0 is false, nonzero is true)"
                );
            }

            checkBlock(whileStmt->body, functionReturnType);
            return;
        }

        if (auto* returnStmt =
                dynamic_cast<const ReturnStmt*>(&statement)) {

            std::string actual = checkExpr(*returnStmt->value);

            if (actual != functionReturnType) {
                throw std::runtime_error("Return type mismatch");
            }

            return;
        }

        if (auto* ifStmt =
                dynamic_cast<const IfStmt*>(&statement)) {

            std::string condType = checkExpr(*ifStmt->condition);

            if (condType != "i32") {
                throw std::runtime_error(
                    "If condition must be i32 (0 is false, nonzero is true)"
                );
            }

            checkBlock(ifStmt->thenBody, functionReturnType);
            checkBlock(ifStmt->elseBody, functionReturnType);
            return;
        }

        throw std::runtime_error("Unknown statement");
    }

private:
    const std::unordered_map<std::string, FunctionSignature>& signatures;
};

// A function's body must be guaranteed to execute a return statement on
// every path, or codegen would fall off the end of its generated
// instructions with no `ret` at all. A while loop is never sufficient
// on its own (it may run zero times); an if/else is only sufficient if
// *both* branches always return.
bool blockAlwaysReturns(
    const std::vector<std::unique_ptr<Statement>>& body
) {
    if (body.empty()) {
        return false;
    }

    const Statement& last = *body.back();

    if (dynamic_cast<const ReturnStmt*>(&last)) {
        return true;
    }

    if (auto* ifStmt = dynamic_cast<const IfStmt*>(&last)) {
        return !ifStmt->elseBody.empty() &&
               blockAlwaysReturns(ifStmt->thenBody) &&
               blockAlwaysReturns(ifStmt->elseBody);
    }

    return false;
}

}  // namespace

void TypeChecker::check(const Program& program) {
    signatures.clear();

    for (const auto& function : program) {
        if (signatures.contains(function.name)) {
            throw std::runtime_error(
                "Function redefined: " + function.name
            );
        }

        FunctionSignature sig;
        for (const auto& param : function.params) {
            sig.paramTypes.push_back(param.type);
        }
        sig.returnType = function.returnType;

        signatures[function.name] = std::move(sig);
    }

    if (!signatures.contains("main")) {
        throw std::runtime_error(
            "Program has no 'main' function"
        );
    }

    for (const auto& function : program) {
        Checker checker(signatures);

        for (const auto& param : function.params) {
            checker.variables[param.name] = param.type;
        }

        checker.checkBlock(function.body, function.returnType);

        if (!blockAlwaysReturns(function.body)) {
            throw std::runtime_error(
                "Function '" + function.name +
                "' does not return on all paths"
            );
        }
    }
}
