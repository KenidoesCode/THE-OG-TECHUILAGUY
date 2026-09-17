#include "type_checker.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class Checker {
public:
    Checker(
        const std::unordered_map<std::string, FunctionSignature>& signatures,
        const StructTable& structs,
        const EnumTable& enums
    )
        : signatures(signatures), structs(structs), enums(enums) {}

    std::unordered_map<std::string, std::string> variables;

    // name -> (element type, size). A separate namespace from
    // `variables`: an array is never used as a plain value, only
    // indexed, so there is no ambiguity in keeping them apart.
    std::unordered_map<std::string, std::pair<std::string, int>> arrays;

    // struct-typed local variable name -> its struct type name. A
    // separate namespace from `variables` for the same reason arrays
    // are kept separate: a struct-typed variable is never used as a
    // plain value (there is no struct-to-struct assignment or
    // pass-by-value yet), only field-accessed.
    std::unordered_map<std::string, std::string> structVars;

    // "constptr" is a read-only view of the same underlying address as
    // "ptr" — a mutable pointer may always be used where a const one is
    // expected (widening), but not the reverse. This is a one-way
    // conversion checked wherever a value flows into a declared/expected
    // type (let initializers, assignments, call arguments, returns), not
    // a distinct runtime representation: both compile to the same raw
    // address, so there is no codegen cost, only a compile-time
    // restriction on where a StoreStmt is allowed to target one.
    static bool isPointerType(const std::string& type) {
        return type == "ptr" || type == "constptr";
    }

    static bool assignable(
        const std::string& declaredType,
        const std::string& actualType
    ) {
        if (declaredType == actualType) {
            return true;
        }

        return declaredType == "constptr" && actualType == "ptr";
    }

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

            if (!isPointerType(pointerType)) {
                throw std::runtime_error(
                    "Cannot dereference a non-pointer value"
                );
            }

            return "i32";
        }

        if (auto* fieldAccess =
                dynamic_cast<const FieldAccessExpr*>(&expr)) {

            // `Name.Variant` (an enum access) reuses the identical
            // dot syntax as `structVar.field` (a struct field access);
            // the two are told apart by whether `structVarName` is a
            // declared struct *variable* or a declared enum *type*
            // name. A struct variable takes priority so a real field
            // access is never misread as an enum lookup.
            if (!structVars.contains(fieldAccess->structVarName)) {
                auto enumIt = enums.find(fieldAccess->structVarName);

                if (enumIt != enums.end()) {
                    if (!enumIt->second.contains(fieldAccess->fieldName)) {
                        throw std::runtime_error(
                            "Enum '" + fieldAccess->structVarName +
                            "' has no variant named: " +
                            fieldAccess->fieldName
                        );
                    }

                    return "i32";
                }
            }

            return resolveFieldType(
                fieldAccess->structVarName,
                fieldAccess->fieldName
            );
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

        if (dynamic_cast<const AsmExpr*>(&expr)) {
            // v1's inline-asm boundary always yields an i32 taken from
            // a fixed register (%eax) after the raw template runs —
            // see docs/ADR/0003-oglang-inline-asm.md. There is nothing
            // to validate about the template text itself; it is
            // opaque to the type checker by design.
            return "i32";
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

                if (!assignable(sig.paramTypes[i], argType)) {
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

    std::string resolveFieldType(
        const std::string& structVarName,
        const std::string& fieldName
    ) {
        auto varIt = structVars.find(structVarName);

        if (varIt == structVars.end()) {
            throw std::runtime_error(
                "Unknown struct variable: " + structVarName
            );
        }

        auto structIt = structs.find(varIt->second);

        if (structIt == structs.end()) {
            throw std::runtime_error(
                "Unknown struct type: " + varIt->second
            );
        }

        auto fieldIt = structIt->second.find(fieldName);

        if (fieldIt == structIt->second.end()) {
            throw std::runtime_error(
                "Struct '" + varIt->second +
                "' has no field named: " + fieldName
            );
        }

        return fieldIt->second;
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

            if (!assignable(letStmt->type, actual)) {
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

            if (!assignable(it->second, actual)) {
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

            if (!isPointerType(pointerType)) {
                throw std::runtime_error(
                    "Cannot store through a non-pointer value"
                );
            }

            if (pointerType == "constptr") {
                throw std::runtime_error(
                    "Cannot store through a const pointer"
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

        if (auto* structVarDecl =
                dynamic_cast<const StructVarDeclStmt*>(&statement)) {

            if (!structs.contains(structVarDecl->structType)) {
                throw std::runtime_error(
                    "Unknown struct type: " + structVarDecl->structType
                );
            }

            structVars[structVarDecl->name] = structVarDecl->structType;
            return;
        }

        if (auto* fieldStore =
                dynamic_cast<const FieldStoreStmt*>(&statement)) {

            std::string fieldType = resolveFieldType(
                fieldStore->structVarName,
                fieldStore->fieldName
            );

            std::string valueType = checkExpr(*fieldStore->value);

            if (!assignable(fieldType, valueType)) {
                throw std::runtime_error(
                    "Type mismatch storing into field '" +
                    fieldStore->fieldName + "' of struct variable: " +
                    fieldStore->structVarName
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

            if (!assignable(functionReturnType, actual)) {
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
    const StructTable& structs;
    const EnumTable& enums;
};

// Base (non-aggregate) types a value can actually have today. A
// struct field's own type is restricted to this set for now — nested
// structs and struct-typed arrays are not supported, a limitation
// this function exists to enforce explicitly rather than let silently
// misbehave in IR lowering.
bool isBaseType(const std::string& type) {
    return type == "i32" || type == "ptr" || type == "constptr";
}

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

TypeChecker::ModuleSymbols TypeChecker::collectModuleSymbols(
    const Program& program
) {
    ModuleSymbols result;

    for (const auto& enumDecl : program.enums) {
        if (result.enums.contains(enumDecl.name)) {
            throw std::runtime_error(
                "Enum redefined: " + enumDecl.name
            );
        }

        std::unordered_map<std::string, int> variants;

        for (size_t i = 0; i < enumDecl.variants.size(); ++i) {
            const std::string& variant = enumDecl.variants[i];

            if (variants.contains(variant)) {
                throw std::runtime_error(
                    "Variant redefined in enum '" + enumDecl.name +
                    "': " + variant
                );
            }

            variants[variant] = static_cast<int>(i);
        }

        result.enums[enumDecl.name] = std::move(variants);
    }

    for (const auto& structDecl : program.structs) {
        if (result.structs.contains(structDecl.name)) {
            throw std::runtime_error(
                "Struct redefined: " + structDecl.name
            );
        }

        if (result.enums.contains(structDecl.name)) {
            throw std::runtime_error(
                "'" + structDecl.name +
                "' is declared as both a struct and an enum"
            );
        }

        std::unordered_map<std::string, std::string> fields;

        for (const auto& field : structDecl.fields) {
            if (fields.contains(field.name)) {
                throw std::runtime_error(
                    "Field redefined in struct '" + structDecl.name +
                    "': " + field.name
                );
            }

            if (!isBaseType(field.type)) {
                throw std::runtime_error(
                    "Struct '" + structDecl.name + "' field '" +
                    field.name + "' has unsupported type: " +
                    field.type +
                    " (nested structs and struct-typed arrays are "
                    "not supported)"
                );
            }

            fields[field.name] = field.type;
        }

        result.structs[structDecl.name] = std::move(fields);
    }

    for (const auto& function : program) {
        if (result.signatures.contains(function.name)) {
            throw std::runtime_error(
                "Function redefined: " + function.name
            );
        }

        FunctionSignature sig;
        for (const auto& param : function.params) {
            // Struct-typed parameters/returns need an ABI (how does a
            // multi-field aggregate get passed/returned?) that hasn't
            // been designed yet — rejected explicitly here rather than
            // silently miscompiled by IR lowering, which only knows
            // how to marshal a single i32/ptr/constptr value per
            // argument.
            if (!isBaseType(param.type)) {
                throw std::runtime_error(
                    "Struct types are not yet supported as function "
                    "parameters: " + function.name + "(" + param.name +
                    ": " + param.type + ")"
                );
            }

            sig.paramTypes.push_back(param.type);
        }

        if (!isBaseType(function.returnType)) {
            throw std::runtime_error(
                "Struct types are not yet supported as function "
                "return types: " + function.name
            );
        }

        sig.returnType = function.returnType;

        result.signatures[function.name] = std::move(sig);
    }

    return result;
}

void TypeChecker::check(const Program& program) {
    if (!program.imports.empty()) {
        throw std::runtime_error(
            "Unresolved import: " + program.imports[0].moduleName +
            " (multi-file compilation is required to resolve an "
            "import — pass every source file on the command line)"
        );
    }

    ModuleSymbols symbols = collectModuleSymbols(program);
    signatures = std::move(symbols.signatures);
    structs = std::move(symbols.structs);
    enums = std::move(symbols.enums);

    if (!signatures.contains("main")) {
        throw std::runtime_error(
            "Program has no 'main' function"
        );
    }

    for (const auto& function : program) {
        Checker checker(signatures, structs, enums);

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

void TypeChecker::checkModule(
    const Program& moduleProgram,
    const std::unordered_map<std::string, FunctionSignature>& externalSignatures,
    const StructTable& externalStructs,
    const EnumTable& externalEnums,
    bool requireMain
) {
    ModuleSymbols own = collectModuleSymbols(moduleProgram);

    signatures = externalSignatures;
    for (auto& [name, sig] : own.signatures) {
        signatures[name] = std::move(sig);
    }

    structs = externalStructs;
    for (auto& [name, fields] : own.structs) {
        structs[name] = std::move(fields);
    }

    enums = externalEnums;
    for (auto& [name, variants] : own.enums) {
        enums[name] = std::move(variants);
    }

    if (requireMain && !signatures.contains("main")) {
        throw std::runtime_error(
            "Program has no 'main' function"
        );
    }

    for (const auto& function : moduleProgram) {
        Checker checker(signatures, structs, enums);

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
