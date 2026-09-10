#pragma once

#include <string>

enum class TypeKind {
    Bool,
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64,
    USize,
    ISize,
    Invalid
};

struct Type {
    TypeKind kind;

    std::string name() const {
        switch (kind) {
            case TypeKind::Bool: return "bool";
            case TypeKind::I8: return "i8";
            case TypeKind::I16: return "i16";
            case TypeKind::I32: return "i32";
            case TypeKind::I64: return "i64";
            case TypeKind::I128: return "i128";
            case TypeKind::U8: return "u8";
            case TypeKind::U16: return "u16";
            case TypeKind::U32: return "u32";
            case TypeKind::U64: return "u64";
            case TypeKind::U128: return "u128";
            case TypeKind::F32: return "f32";
            case TypeKind::F64: return "f64";
            case TypeKind::USize: return "usize";
            case TypeKind::ISize: return "isize";
            case TypeKind::Invalid: return "invalid";
        }

        return "invalid";
    }
};
