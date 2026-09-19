#pragma once

#include <vector>

// The first real end-to-end training demonstration built on
// ../autodiff/value.hpp — see docs/ADR/0024-ai-tensor-autodiff-foundation.md.
// Trains exactly one model shape (y = weight*x + bias) via plain
// gradient descent on mean-squared error. Not a general training-loop
// abstraction.

namespace ai {

struct TrainingResult {
    double weight = 0.0;
    double bias = 0.0;
    std::vector<double> lossHistory;  // mean-squared-error per epoch
};

// Trains weight/bias to fit (xs[i], ys[i]) pairs via `epochs` steps of
// plain gradient descent at the given `learningRate`, starting from
// weight=0, bias=0.
TrainingResult trainLinearRegression(
    const std::vector<double>& xs, const std::vector<double>& ys,
    double learningRate, int epochs
);

// Evaluates a trained model at a single input — the "inference" half
// of the train -> infer loop.
double predict(const TrainingResult& model, double x);

}  // namespace ai
