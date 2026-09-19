#pragma once

#include <cstddef>
#include <vector>

// A small, real, shape-checked numeric container — no gradient
// tracking (see docs/ADR/0024-ai-tensor-autodiff-foundation.md for
// exactly why autodiff is a separate, scalar-only engine in this
// slice, not built into Tensor).

namespace ai {

class Tensor {
public:
    explicit Tensor(std::vector<size_t> shape);
    Tensor(std::vector<size_t> shape, std::vector<double> data);

    const std::vector<size_t>& shape() const { return shapeVec; }
    size_t size() const { return data_.size(); }
    size_t rank() const { return shapeVec.size(); }

    double& at(size_t flatIndex) { return data_[flatIndex]; }
    double at(size_t flatIndex) const { return data_[flatIndex]; }

    // Rank-2 convenience accessors. Undefined (like std::vector::operator[])
    // if rank() != 2 or the indices are out of range — callers needing
    // safety should check rank()/shape() first, the same discipline
    // this project applies elsewhere to unchecked-by-design accessors.
    double& at2d(size_t row, size_t col) { return data_[row * shapeVec[1] + col]; }
    double at2d(size_t row, size_t col) const { return data_[row * shapeVec[1] + col]; }

    static bool sameShape(const Tensor& a, const Tensor& b);

private:
    std::vector<size_t> shapeVec;
    std::vector<double> data_;
};

// Each returns false (leaving `out` unspecified) on a shape mismatch —
// no broadcasting is implemented (see the ADR).
bool add(const Tensor& a, const Tensor& b, Tensor& out);
bool subtract(const Tensor& a, const Tensor& b, Tensor& out);
bool multiplyElementwise(const Tensor& a, const Tensor& b, Tensor& out);

Tensor scale(const Tensor& t, double factor);

// Real 2D matrix multiplication (a: [m,k], b: [k,n] -> out: [m,n]).
// Returns false (leaving `out` unspecified) if either tensor isn't
// rank 2 or the inner dimensions don't match.
bool matmul(const Tensor& a, const Tensor& b, Tensor& out);

}  // namespace ai
