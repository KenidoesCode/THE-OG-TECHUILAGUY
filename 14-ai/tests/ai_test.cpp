// Real assertion-based tests for the AI Tensor + scalar-autodiff
// foundation (14-ai/) — hand-computed expected values, a numerical
// (finite-difference) cross-check of the autodiff engine, and a real
// end-to-end training convergence test. See
// docs/ADR/0024-ai-tensor-autodiff-foundation.md.

#include "../autodiff/value.hpp"
#include "../tensor/tensor.hpp"
#include "../training/linear_regression.hpp"

#include <cmath>
#include <iostream>
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

bool nearlyEqual(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) < tolerance;
}

}  // namespace

void testTensorShapeAndSize() {
    ai::Tensor t(std::vector<size_t>{2, 3});
    check(t.rank() == 2, "tensor: rank() reports the correct number of dimensions");
    check(t.size() == 6, "tensor: size() is the product of the shape (2*3=6)");
    check(t.shape()[0] == 2 && t.shape()[1] == 3, "tensor: shape() reports the exact dimensions given");

    ai::Tensor fresh(std::vector<size_t>{2, 2});
    check(fresh.at(0) == 0.0 && fresh.at(3) == 0.0, "tensor: a freshly-constructed tensor is zero-initialized");
}

void testElementwiseOperations() {
    ai::Tensor a(std::vector<size_t>{2, 2}, std::vector<double>{1, 2, 3, 4});
    ai::Tensor b(std::vector<size_t>{2, 2}, std::vector<double>{10, 20, 30, 40});

    ai::Tensor sum(std::vector<size_t>{1});
    check(ai::add(a, b, sum), "tensor: adding two matching-shape tensors succeeds");
    check(sum.at(0) == 11 && sum.at(1) == 22 && sum.at(2) == 33 && sum.at(3) == 44,
          "tensor: elementwise addition produces the hand-computed expected values");

    ai::Tensor diff(std::vector<size_t>{1});
    check(ai::subtract(b, a, diff), "tensor: subtracting two matching-shape tensors succeeds");
    check(diff.at(0) == 9 && diff.at(3) == 36, "tensor: elementwise subtraction produces the hand-computed expected values");

    ai::Tensor product(std::vector<size_t>{1});
    check(ai::multiplyElementwise(a, b, product), "tensor: elementwise-multiplying two matching-shape tensors succeeds");
    check(product.at(0) == 10 && product.at(3) == 160,
          "tensor: elementwise multiplication produces the hand-computed expected values");

    ai::Tensor scaled = ai::scale(a, 2.0);
    check(scaled.at(0) == 2 && scaled.at(3) == 8, "tensor: scale() multiplies every element by the given factor");
}

void testShapeMismatchIsRejected() {
    ai::Tensor a(std::vector<size_t>{2, 2});
    ai::Tensor b(std::vector<size_t>{3, 3});
    ai::Tensor out(std::vector<size_t>{1});

    check(!ai::add(a, b, out), "tensor: adding mismatched shapes fails (returns false), not a crash or silent truncation");
    check(!ai::subtract(a, b, out), "tensor: subtracting mismatched shapes fails cleanly");
    check(!ai::multiplyElementwise(a, b, out), "tensor: elementwise-multiplying mismatched shapes fails cleanly");
}

void testMatmul() {
    // [1 2 3]   [ 7  8]
    // [4 5 6] x [ 9 10]  = [1*7+2*9+3*11, 1*8+2*10+3*12; 4*7+5*9+6*11, 4*8+5*10+6*12]
    //           [11 12]
    ai::Tensor a(std::vector<size_t>{2, 3}, std::vector<double>{1, 2, 3, 4, 5, 6});
    ai::Tensor b(std::vector<size_t>{3, 2}, std::vector<double>{7, 8, 9, 10, 11, 12});
    ai::Tensor out(std::vector<size_t>{1});

    check(ai::matmul(a, b, out), "tensor: matmul on compatible (2x3)*(3x2) shapes succeeds");
    check(out.shape()[0] == 2 && out.shape()[1] == 2, "tensor: matmul's output shape is [rows(a), cols(b)]");
    check(nearlyEqual(out.at2d(0, 0), 58) && nearlyEqual(out.at2d(0, 1), 64) &&
              nearlyEqual(out.at2d(1, 0), 139) && nearlyEqual(out.at2d(1, 1), 154),
          "tensor: matmul produces the hand-computed expected result for a non-square case");

    ai::Tensor incompatible(std::vector<size_t>{2, 2});
    check(!ai::matmul(a, incompatible, out), "tensor: matmul rejects mismatched inner dimensions, not a crash");

    ai::Tensor notRank2(std::vector<size_t>{2, 2, 2});
    check(!ai::matmul(notRank2, b, out), "tensor: matmul rejects a non-rank-2 operand");
}

void testAutodiffBasicOperations() {
    // f(x, y) = x*y + x, at x=3, y=4: f=15, df/dx = y+1 = 5, df/dy = x = 3
    autodiff::Value x(3.0);
    autodiff::Value y(4.0);
    autodiff::Value f = x * y + x;
    check(nearlyEqual(f.data(), 15.0), "autodiff: forward evaluation of x*y+x at x=3,y=4 is the hand-computed 15");

    f.backward();
    check(nearlyEqual(x.grad(), 5.0), "autodiff: d(x*y+x)/dx at x=3,y=4 matches the hand-derived value 5 (=y+1)");
    check(nearlyEqual(y.grad(), 3.0), "autodiff: d(x*y+x)/dy at x=3,y=4 matches the hand-derived value 3 (=x)");
}

void testAutodiffAccumulatesGradientForReusedValue() {
    // f(x) = x*x, at x=5: f=25, df/dx = 2x = 10 — this only comes out
    // correctly if the SAME node's gradient contributions from both
    // "slots" of the multiplication are accumulated (+=), not
    // overwritten.
    autodiff::Value x(5.0);
    autodiff::Value f = x * x;
    check(nearlyEqual(f.data(), 25.0), "autodiff: forward evaluation of x*x at x=5 is 25");

    f.backward();
    check(nearlyEqual(x.grad(), 10.0),
          "autodiff: d(x*x)/dx at x=5 correctly accumulates to 10 (=2x), proving reused-node gradient accumulation works");
}

void testAutodiffUnaryAndBinaryMinus() {
    autodiff::Value a(7.0);
    autodiff::Value b(2.0);
    autodiff::Value diff = a - b;
    check(nearlyEqual(diff.data(), 5.0), "autodiff: a-b at a=7,b=2 evaluates to 5");

    diff.backward();
    check(nearlyEqual(a.grad(), 1.0) && nearlyEqual(b.grad(), -1.0),
          "autodiff: d(a-b) gradients are exactly +1 for a and -1 for b");
}

void testAutodiffMatchesNumericalGradient() {
    // g(x, y) = (x + y) * (x - y)  [= x^2 - y^2]
    // Analytic: dg/dx = 2x, dg/dy = -2y. Cross-checked here against
    // real central-difference numerical gradients, independently of
    // the analytic derivation above.
    double xVal = 3.0;
    double yVal = 1.5;

    autodiff::Value x(xVal);
    autodiff::Value y(yVal);
    autodiff::Value g = (x + y) * (x - y);
    g.backward();

    double h = 1e-6;
    auto evalG = [](double xv, double yv) {
        autodiff::Value xx(xv);
        autodiff::Value yy(yv);
        return ((xx + yy) * (xx - yy)).data();
    };
    double numericalDx = (evalG(xVal + h, yVal) - evalG(xVal - h, yVal)) / (2 * h);
    double numericalDy = (evalG(xVal, yVal + h) - evalG(xVal, yVal - h)) / (2 * h);

    check(std::fabs(x.grad() - numericalDx) < 1e-4,
          "autodiff: the analytic gradient w.r.t. x matches an independently-computed numerical (finite-difference) gradient");
    check(std::fabs(y.grad() - numericalDy) < 1e-4,
          "autodiff: the analytic gradient w.r.t. y matches an independently-computed numerical (finite-difference) gradient");
}

void testLinearRegressionConvergesToTrueParameters() {
    // y = 2x + 3, exactly (noise-free) — a fixed, reproducible dataset.
    std::vector<double> xs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<double> ys;
    for (double x : xs) ys.push_back(2.0 * x + 3.0);

    ai::TrainingResult model = ai::trainLinearRegression(xs, ys, 0.01, 2000);

    check(std::fabs(model.weight - 2.0) < 0.05,
          "ai: linear regression trained weight converges to within 0.05 of the true value 2.0");
    check(std::fabs(model.bias - 3.0) < 0.05,
          "ai: linear regression trained bias converges to within 0.05 of the true value 3.0");
    check(model.lossHistory.front() > model.lossHistory.back(),
          "ai: training loss decreases from the first epoch to the last");
    check(model.lossHistory.back() < 0.01, "ai: final training loss is small (the model actually fits the data)");

    double predicted = ai::predict(model, 10.0);
    check(std::fabs(predicted - 23.0) < 0.5,
          "ai: inference on an unseen input (x=10) is close to the true value (2*10+3=23) — the model generalizes, not just memorizes training points");
}

int main() {
    testTensorShapeAndSize();
    testElementwiseOperations();
    testShapeMismatchIsRejected();
    testMatmul();
    testAutodiffBasicOperations();
    testAutodiffAccumulatesGradientForReusedValue();
    testAutodiffUnaryAndBinaryMinus();
    testAutodiffMatchesNumericalGradient();
    testLinearRegressionConvergesToTrueParameters();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
