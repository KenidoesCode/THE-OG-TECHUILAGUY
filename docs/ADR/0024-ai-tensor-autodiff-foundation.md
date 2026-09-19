# ADR 0024: AI — Tensor + scalar autodiff foundation, linear regression demo

**Status:** Accepted. This is the FOUNDATION vertical slice of PRD
Layer 14 (AI/ML). It builds a small, real `Tensor` type (shape,
storage, elementwise ops, matrix multiplication) and a small, real
reverse-mode **scalar** autodiff engine, used together to actually
train a linear regression model (`y = 2x + 3`) end to end. This is
explicitly **not** a general-purpose ML framework, not a neural
network, and not Tensor-level automatic differentiation — both scope
limits are deliberate and named below, not gaps discovered later.

## Context

The project's own rule ("what is the smallest real implementation that
establishes the architectural foundation") applies directly here: AI
needs a real numeric container with real operations, and a real
mechanism for computing gradients, before anything resembling a neural
network is meaningful. Rather than build a large, general
Tensor-autograd system (a multi-week undertaking on its own) and risk
shipping something half-real, this slice deliberately splits the two
concerns:

1. **`Tensor`** (`14-ai/tensor/tensor.hpp/cpp`): a real, tested,
   shape-checked numeric container with elementwise operations and 2D
   matrix multiplication — no gradient tracking at all.
2. **`autodiff::Value`** (`14-ai/autodiff/value.hpp/cpp`): a real,
   tested, reverse-mode **scalar** autodiff engine (the same
   architecture popularized by micrograd-style engines) — each `Value`
   is a node in a dynamically-built computation DAG; `backward()`
   topologically sorts that DAG and accumulates gradients via the
   chain rule.

The linear regression training demo (`14-ai/training/linear_regression.hpp/cpp`)
is built on `autodiff::Value` directly (each training example's
prediction is a small scalar expression: `w * x + b`), **not** by
running gradients through `Tensor` operations — extending autodiff to
`Tensor` itself is explicitly named as future work, not silently
assumed to already exist.

## Tensor

`Tensor(shape, data)`: a flat, row-major `std::vector<double>` plus a
shape vector. `size()` is the product of the shape. Elementwise
`add`/`subtract`/`multiplyElementwise` take two tensors and an output
reference, returning `false` (leaving `out` untouched) on a shape
mismatch — **no broadcasting is implemented**; both operands must have
the identical shape, full stop. `matmul(a, b, out)` requires both `a`
and `b` to be rank-2 with `a`'s column count equal to `b`'s row count,
returning `false` otherwise; the actual multiplication is the standard
`O(m·k·n)` triple loop — correct, not optimized (no blocking, no SIMD,
no BLAS).

## Scalar autodiff

`autodiff::Value` wraps a `shared_ptr<Node>` (`data`, `grad`, a
`backwardFn` closure, and a list of parent nodes) so multiple `Value`
copies can share and mutate the same underlying node — required for
correct gradient **accumulation** when one value is used more than
once in an expression (e.g. `diff * diff` for a squared-error term, or
reusing the same weight `Value` across every training example in an
epoch). Supported operations: `+`, unary `-`, `-` (binary, via
`+`/unary `-`), `*`. Each operation's `backwardFn` uses `+=` on its
operands' `grad` fields (never `=`), which is exactly what makes
correct accumulation across shared/reused nodes possible.

`backward()`: zeros every reachable node's `grad`, seeds the calling
node's own `grad` to `1.0`, computes a real topological order via DFS
(a node is only added to the order after all of its parents have been
visited — reverse-mode requires processing outputs before their
inputs), then calls each node's `backwardFn` in reverse topological
order. This is a real implementation of the reverse-mode algorithm,
verified against independently-computed **numerical** (finite-
difference) gradients in the test suite — not merely asserted to be
correct by construction.

## Linear regression training demo

`trainLinearRegression(xs, ys, learningRate, epochs)`: initializes
`weight = 0`, `bias = 0` as plain doubles; each epoch, wraps the
current values as fresh `Value` leaves, builds the mean-squared-error
loss over every `(x, y)` pair (`Σ (w·x + b − y)² / n`) as a single
`Value` expression graph, calls `backward()` once, then updates the raw
`weight`/`bias` doubles by `learningRate * grad` (plain gradient
descent — no momentum, no Adam, no learning-rate schedule). The
demonstration trains on synthetic data generated from `y = 2x + 3`
and is asserted to converge to `weight ≈ 2` and `bias ≈ 3` within a
documented tolerance (`0.05` after a fixed, documented number of
epochs) — a real, reproducible convergence proof, not a hand-picked
lucky run (the test uses a fixed dataset and fixed epoch count, so it
either converges within tolerance every time or the test fails).

## What this does not support

- **No Tensor-level autodiff.** `Tensor`'s operations never build a
  computation graph or track gradients; only scalar `Value` does.
  Making `Tensor` itself differentiable is a real, separate, larger
  piece of future work.
- **No broadcasting.** Elementwise `Tensor` operations require
  identical shapes; there is no NumPy-style shape broadcasting.
- **No neural network layers, activation functions, optimizers beyond
  plain gradient descent, loss functions beyond MSE, or a training
  loop abstraction.** This slice trains exactly one model shape
  (`y = wx + b`) end to end; nothing here generalizes to an arbitrary
  model architecture yet.
- **No GPU/accelerator support, no batching, no vectorized/BLAS-backed
  matmul.** `matmul` is a correct but naive triple loop.
- **No serialization/model saving, no inference-only fast path** beyond
  simply evaluating `weight * x + bias` with the trained scalars.

## Tested invariants

`14-ai/tests/ai_test.cpp`: `Tensor` shape/size correctness; elementwise
add/subtract/multiply on matching shapes with hand-computed expected
results; elementwise operations correctly rejecting a shape mismatch
(returning `false`, not crashing or silently truncating); `matmul` on
real, hand-verified small matrices (including a non-square case) and
correctly rejecting mismatched inner dimensions; `autodiff::Value`
correctly computing gradients for `+`, `*`, and unary `-` against
**hand-derived** expected values for a small fixed expression;
gradient **accumulation** when the same `Value` is used twice in one
expression (`x*x`'s gradient is `2x`, not `x`, proving reused-node
accumulation actually works, not just single-use chains); a
numerical-gradient (finite-difference) cross-check against the
autodiff-computed gradient for a small multi-variable expression,
independently confirming `backward()`'s correctness rather than
trusting the analytic derivation alone; and the full linear-regression
training demo converging to `weight`/`bias` within the documented
`0.05` tolerance of the true generating values `2`/`3` from a fixed
synthetic dataset and fixed epoch count.

## Consequences

THE OG TECHUILAGUY now has a real, tested numeric `Tensor` primitive
and a real, tested, numerically-cross-checked reverse-mode autodiff
engine capable of actually training a (simple, scalar) model to
convergence — a genuine "tensor → operation → autodiff → training →
inference" loop, at exactly the scope this ADR claims and no further.
The next real gaps, in roughly increasing order: extending autodiff to
operate over `Tensor` (not just scalars), a real neural-network layer
abstraction on top of that, then a proper optimizer/training-loop
abstraction — none of which exist yet. This is explicitly the first
vertical slice of a much larger domain, not a general-purpose ML
framework.
