#include "tensor.hpp"

namespace ai {

namespace {

size_t productOf(const std::vector<size_t>& shape) {
    size_t product = 1;
    for (size_t dim : shape) product *= dim;
    return product;
}

}  // namespace

Tensor::Tensor(std::vector<size_t> shape)
    : shapeVec(std::move(shape)), data_(productOf(shapeVec), 0.0) {}

Tensor::Tensor(std::vector<size_t> shape, std::vector<double> data)
    : shapeVec(std::move(shape)), data_(std::move(data)) {}

bool Tensor::sameShape(const Tensor& a, const Tensor& b) {
    return a.shapeVec == b.shapeVec;
}

bool add(const Tensor& a, const Tensor& b, Tensor& out) {
    if (!Tensor::sameShape(a, b)) return false;
    Tensor result(a.shape());
    for (size_t i = 0; i < a.size(); ++i) result.at(i) = a.at(i) + b.at(i);
    out = std::move(result);
    return true;
}

bool subtract(const Tensor& a, const Tensor& b, Tensor& out) {
    if (!Tensor::sameShape(a, b)) return false;
    Tensor result(a.shape());
    for (size_t i = 0; i < a.size(); ++i) result.at(i) = a.at(i) - b.at(i);
    out = std::move(result);
    return true;
}

bool multiplyElementwise(const Tensor& a, const Tensor& b, Tensor& out) {
    if (!Tensor::sameShape(a, b)) return false;
    Tensor result(a.shape());
    for (size_t i = 0; i < a.size(); ++i) result.at(i) = a.at(i) * b.at(i);
    out = std::move(result);
    return true;
}

Tensor scale(const Tensor& t, double factor) {
    Tensor result(t.shape());
    for (size_t i = 0; i < t.size(); ++i) result.at(i) = t.at(i) * factor;
    return result;
}

bool matmul(const Tensor& a, const Tensor& b, Tensor& out) {
    if (a.rank() != 2 || b.rank() != 2) return false;
    size_t m = a.shape()[0];
    size_t k = a.shape()[1];
    size_t kb = b.shape()[0];
    size_t n = b.shape()[1];
    if (k != kb) return false;

    Tensor result(std::vector<size_t>{m, n});
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double sum = 0.0;
            for (size_t t = 0; t < k; ++t) {
                sum += a.at2d(i, t) * b.at2d(t, j);
            }
            result.at2d(i, j) = sum;
        }
    }
    out = std::move(result);
    return true;
}

}  // namespace ai
