#include "value.hpp"

#include <set>

namespace autodiff {

Value::Value(double data) : node(std::make_shared<Node>()) {
    node->data = data;
}

Value Value::operator+(const Value& other) const {
    auto result = std::make_shared<Node>();
    result->data = node->data + other.node->data;
    result->parents = {node, other.node};

    std::shared_ptr<Node> self = node;
    std::shared_ptr<Node> rhs = other.node;
    std::weak_ptr<Node> weakResult = result;
    result->backwardFn = [self, rhs, weakResult]() {
        auto r = weakResult.lock();
        self->grad += r->grad;
        rhs->grad += r->grad;
    };
    return Value(result);
}

Value Value::operator-() const {
    auto result = std::make_shared<Node>();
    result->data = -node->data;
    result->parents = {node};

    std::shared_ptr<Node> self = node;
    std::weak_ptr<Node> weakResult = result;
    result->backwardFn = [self, weakResult]() {
        auto r = weakResult.lock();
        self->grad += -r->grad;
    };
    return Value(result);
}

Value Value::operator-(const Value& other) const {
    return *this + (-other);
}

Value Value::operator*(const Value& other) const {
    auto result = std::make_shared<Node>();
    result->data = node->data * other.node->data;
    result->parents = {node, other.node};

    std::shared_ptr<Node> self = node;
    std::shared_ptr<Node> rhs = other.node;
    std::weak_ptr<Node> weakResult = result;
    result->backwardFn = [self, rhs, weakResult]() {
        auto r = weakResult.lock();
        self->grad += rhs->data * r->grad;
        rhs->grad += self->data * r->grad;
    };
    return Value(result);
}

namespace {

void zeroGradsRecursive(const std::shared_ptr<Node>& n, std::set<Node*>& visited) {
    if (visited.count(n.get()) > 0) return;
    visited.insert(n.get());
    n->grad = 0.0;
    for (const auto& parent : n->parents) {
        zeroGradsRecursive(parent, visited);
    }
}

void buildTopoOrder(
    const std::shared_ptr<Node>& n, std::set<Node*>& visited,
    std::vector<std::shared_ptr<Node>>& order
) {
    if (visited.count(n.get()) > 0) return;
    visited.insert(n.get());
    for (const auto& parent : n->parents) {
        buildTopoOrder(parent, visited, order);
    }
    order.push_back(n);
}

}  // namespace

void Value::backward() {
    std::set<Node*> zeroVisited;
    zeroGradsRecursive(node, zeroVisited);

    std::set<Node*> topoVisited;
    std::vector<std::shared_ptr<Node>> order;
    buildTopoOrder(node, topoVisited, order);

    node->grad = 1.0;
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        (*it)->backwardFn();
    }
}

}  // namespace autodiff
