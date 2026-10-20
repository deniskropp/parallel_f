// parallel_f_gp_tree_interpreter.cpp
// Tree-based Genetic Programming interpreter
// Parallel fitness evaluation via parallel_f::task_list
// Compile: C++17 -pthread against parallel_f.hpp

#include "parallel_f.hpp"
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <functional>
#include <limits>

// ---------------------------------------------------------------------------
// GP Tree Node
// ---------------------------------------------------------------------------
enum class NodeType { TerminalX, TerminalConst, Add, Sub, Mul, Div, Sin, Cos };

struct GPNode {
    NodeType type;
    double value = 0.0;                     // for TerminalConst
    std::vector<std::unique_ptr<GPNode>> children;

    GPNode(NodeType t, double v = 0.0) : type(t), value(v) {}

    // Deep copy
    std::unique_ptr<GPNode> clone() const {
        auto n = std::make_unique<GPNode>(type, value);
        for (const auto& c : children)
            n->children.push_back(c->clone());
        return n;
    }

    // Recursive evaluation
    double eval(double x) const {
        switch (type) {
            case NodeType::TerminalX:     return x;
            case NodeType::TerminalConst: return value;
            case NodeType::Add:  return children[0]->eval(x) + children[1]->eval(x);
            case NodeType::Sub:  return children[0]->eval(x) - children[1]->eval(x);
            case NodeType::Mul:  return children[0]->eval(x) * children[1]->eval(x);
            case NodeType::Div: {
                double d = children[1]->eval(x);
                return (std::abs(d) < 1e-6) ? 1.0 : children[0]->eval(x) / d;
            }
            case NodeType::Sin:  return std::sin(children[0]->eval(x));
            case NodeType::Cos:  return std::cos(children[0]->eval(x));
        }
        return 0.0;
    }

    size_t size() const {
        size_t s = 1;
        for (const auto& c : children) s += c->size();
        return s;
    }
};

// ---------------------------------------------------------------------------
// Individual with tree genome
// ---------------------------------------------------------------------------
struct Individual {
    std::unique_ptr<GPNode> tree;
    double fitness = std::numeric_limits<double>::max();

    Individual() = default;
    Individual(std::unique_ptr<GPNode> t) : tree(std::move(t)) {}

    Individual(const Individual& o) : fitness(o.fitness) {
        if (o.tree) tree = o.tree->clone();
    }
    Individual& operator=(const Individual& o) {
        if (this != &o) {
            fitness = o.fitness;
            tree = o.tree ? o.tree->clone() : nullptr;
        }
        return *this;
    }
    Individual(Individual&&) = default;
    Individual& operator=(Individual&&) = default;

    double evaluate(const std::vector<double>& xs,
                    const std::vector<double>& ys) const {
        if (!tree) return std::numeric_limits<double>::max();
        double err = 0.0;
        for (size_t i = 0; i < xs.size(); ++i) {
            double pred = tree->eval(xs[i]);
            err += std::pow(pred - ys[i], 2);
        }
        return err / xs.size();   // MSE
    }
};

// ---------------------------------------------------------------------------
// Random tree generation (ramped half-and-half style)
// ---------------------------------------------------------------------------
std::unique_ptr<GPNode> random_tree(std::mt19937& rng, int max_depth, bool full = false) {
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::uniform_real_distribution<double> const_dist(-5.0, 5.0);

    auto is_terminal = [&](int depth) {
        return depth >= max_depth || (!full && depth > 1 && u01(rng) < 0.3);
    };

    std::function<std::unique_ptr<GPNode>(int)> gen = [&](int depth) -> std::unique_ptr<GPNode> {
        if (is_terminal(depth)) {
            if (u01(rng) < 0.5)
                return std::make_unique<GPNode>(NodeType::TerminalX);
            else
                return std::make_unique<GPNode>(NodeType::TerminalConst, const_dist(rng));
        }

        // Function nodes (binary or unary)
        NodeType funcs[] = {NodeType::Add, NodeType::Sub, NodeType::Mul,
                            NodeType::Div, NodeType::Sin, NodeType::Cos};
        NodeType f = funcs[std::uniform_int_distribution<int>(0, 5)(rng)];

        auto node = std::make_unique<GPNode>(f);
        int arity = (f == NodeType::Sin || f == NodeType::Cos) ? 1 : 2;
        for (int i = 0; i < arity; ++i)
            node->children.push_back(gen(depth + 1));
        return node;
    };

    return gen(0);
}

// ---------------------------------------------------------------------------
// Parallel fitness evaluation (unchanged contract with parallel_f)
// ---------------------------------------------------------------------------
void evaluate_population_parallel(
    std::vector<Individual>& population,
    const std::vector<double>& xs,
    const std::vector<double>& ys)
{
    parallel_f::task_list tl;

    for (auto& ind : population) {
        auto task = parallel_f::make_task([&ind, &xs, &ys]() {
            ind.fitness = ind.evaluate(xs, ys);
        });
        tl.append(task);
    }

    tl.finish();
    parallel_f::stats::instance::get().show_stats();
}

// ---------------------------------------------------------------------------
// Main generational loop
// ---------------------------------------------------------------------------
int main()
{
    constexpr size_t POP_SIZE    = 128;
    constexpr int    MAX_DEPTH   = 5;
    constexpr size_t GENERATIONS = 30;
    constexpr size_t DATA_POINTS = 50;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> u01(0.0, 1.0);

    // Target: y = x^2 + sin(x)
    std::vector<double> xs(DATA_POINTS), ys(DATA_POINTS);
    for (size_t i = 0; i < DATA_POINTS; ++i) {
        xs[i] = -3.0 + 6.0 * i / (DATA_POINTS - 1);
        ys[i] = xs[i] * xs[i] + std::sin(xs[i]);
    }

    // Initialize population with random trees
    std::vector<Individual> population;
    population.reserve(POP_SIZE);
    for (size_t i = 0; i < POP_SIZE; ++i)
        population.emplace_back(random_tree(rng, MAX_DEPTH, i % 2 == 0));

    for (size_t gen = 0; gen < GENERATIONS; ++gen) {
        evaluate_population_parallel(population, xs, ys);

        std::sort(population.begin(), population.end(),
                  [](const Individual& a, const Individual& b) {
                      return a.fitness < b.fitness;
                  });

        std::cout << "Gen " << gen
                  << "  best MSE: " << population[0].fitness
                  << "  tree size: " << (population[0].tree ? population[0].tree->size() : 0)
                  << std::endl;

        // Elitism + replace worst half with mutated clones of elites
        for (size_t i = POP_SIZE / 2; i < POP_SIZE; ++i) {
            size_t parent = i % (POP_SIZE / 4);
            population[i] = population[parent];

            // Simple subtree mutation (replace a random subtree)
            if (population[i].tree && u01(rng) < 0.4) {
                // For brevity: regenerate whole tree with small probability
                // (full subtree selection can be added later)
                if (u01(rng) < 0.15)
                    population[i].tree = random_tree(rng, MAX_DEPTH);
            }
        }
    }

    std::cout << "\nFinal best MSE: " << population[0].fitness << std::endl;
    return 0;
}