#include "linear_regression.hpp"

#include "../autodiff/value.hpp"

namespace ai {

TrainingResult trainLinearRegression(
    const std::vector<double>& xs, const std::vector<double>& ys,
    double learningRate, int epochs
) {
    TrainingResult result;
    double weightRaw = 0.0;
    double biasRaw = 0.0;
    size_t n = xs.size();

    for (int epoch = 0; epoch < epochs; ++epoch) {
        autodiff::Value weight(weightRaw);
        autodiff::Value bias(biasRaw);
        autodiff::Value totalLoss(0.0);

        for (size_t i = 0; i < n; ++i) {
            autodiff::Value x(xs[i]);
            autodiff::Value y(ys[i]);
            autodiff::Value prediction = weight * x + bias;
            autodiff::Value diff = prediction - y;
            autodiff::Value squaredError = diff * diff;
            totalLoss = totalLoss + squaredError;
        }
        autodiff::Value meanLoss = totalLoss * autodiff::Value(1.0 / static_cast<double>(n));

        meanLoss.backward();

        weightRaw -= learningRate * weight.grad();
        biasRaw -= learningRate * bias.grad();

        result.lossHistory.push_back(meanLoss.data());
    }

    result.weight = weightRaw;
    result.bias = biasRaw;
    return result;
}

double predict(const TrainingResult& model, double x) {
    return model.weight * x + model.bias;
}

}  // namespace ai
