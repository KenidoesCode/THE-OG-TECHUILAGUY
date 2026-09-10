#pragma once

#include <string>

enum class TypeKind {
    I32,
    Invalid
};

struct Type {
    TypeKind kind;

    std::string name() const {
        switch (kind) {
            case TypeKind::I32: return "i32";
            default: return "invalid";
        }
    }
};
