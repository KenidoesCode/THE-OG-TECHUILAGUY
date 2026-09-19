#pragma once

#include <functional>
#include <memory>
#include <vector>

// A small, real reverse-mode SCALAR autodiff engine — the same
// architecture popularized by micrograd-style engines: each Value
// wraps a shared graph node so copies of the same Value correctly
// share (and accumulate gradient into) one underlying node. This is
// deliberately scalar-only; Tensor (../tensor/tensor.hpp) has no
// gradient tracking in this slice. See
// docs/ADR/0024-ai-tensor-autodiff-foundation.md.

namespace autodiff {

struct Node {
    double data = 0.0;
    double grad = 0.0;
    std::function<void()> backwardFn = [] {};
    std::vector<std::shared_ptr<Node>> parents;
};

class Value {
public:
    explicit Value(double data);

    double data() const { return node->data; }
    double grad() const { return node->grad; }

    Value operator+(const Value& other) const;
    Value operator-() const;
    Value operator-(const Value& other) const;
    Value operator*(const Value& other) const;

    // Zeros every reachable node's gradient, seeds this node's own
    // gradient to 1.0, computes a real topological order (DFS,
    // parents-before-self), then calls every node's backwardFn in
    // reverse topological order — a real implementation of the
    // reverse-mode algorithm, not asserted-correct-by-construction
    // (see the ADR's numerical-gradient cross-check test).
    void backward();

    const std::shared_ptr<Node>& rawNode() const { return node; }

private:
    explicit Value(std::shared_ptr<Node> n) : node(std::move(n)) {}

    std::shared_ptr<Node> node;
};

}  // namespace autodiff
