// generated_gp_parallel.cpp
// Generated from KickLang Parallel DAG: gp_tree_evolution
// Thin parallel_f mapping — fitness fan-out + dependent variation stages
// Compile: g++ -std=c++17 -pthread -O2 generated_gp_parallel.cpp -o gp_parallel

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
// GP Tree Node + Individual (from prior interpreter cycle)
// ---------------------------------------------------------------------------
enum class NodeType { TerminalX, TerminalConst, Add, Sub, Mul, Div, Sin, Cos };

struct GPNode {
    NodeType type;
    double value = 0.0;
    std::vector<std::unique_ptr<GPNode>> children;

    GPNode(NodeType t, double v = 0.0) : type(t), value(v) {}

    std::unique_ptr<GPNode> clone() const {
        auto n = std::make_unique<GPNode>(type, value);
        for (const auto& c : children)
            n->children.push_back(c->clone());
        return n;
    }

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

struct Individual {
    std::unique_ptr<GPNode> tree;
    double fitness = std::numeric_limits<double>::max();

    Individual() = default;
    explicit Individual(std::unique_ptr<GPNode> t) : tree(std::move(t)) {}

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

    double evaluate(const std::vector<double>& xs, const std::vector<double>& ys) const {
        if (!tree) return std::numeric_limits<double>::max();
        double err = 0.0;
        for (size_t i = 0; i < xs.size(); ++i) {
            double pred = tree->eval(xs[i]);
            err += std::pow(pred - ys[i], 2);
        }
        return err / xs.size();
    }
};

// ---------------------------------------------------------------------------
// Random tree generation
// ---------------------------------------------------------------------------
std::unique_ptr<GPNode> random_tree(std::mt19937& rng, int max_depth, bool full = false) {
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::uniform_real_distribution<double> const_dist(-5.0, 5.0);

    auto is_terminal = [&](int depth) {
        return depth >= max_depth || (!full && depth > 1 && u01(rng) < 0.3);
    };

    std::function<std::unique_ptr<GPNode>(int)> gen = [&](int depth) -> std::unique_ptr<GPNode> {
        if (is_terminal(depth)) {
            return (u01(rng) < 0.5)
                ? std::make_unique<GPNode>(NodeType::TerminalX)
                : std::make_unique<GPNode>(NodeType::TerminalConst, const_dist(rng));
        }
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
// Subtree operators (from prior cycle)
// ---------------------------------------------------------------------------
struct NodeRef { GPNode* node = nullptr; GPNode* parent = nullptr; size_t child_index = 0; };

size_t count_nodes(GPNode* n) {
    if (!n) return 0;
    size_t c = 1;
    for (auto& ch : n->children) c += count_nodes(ch.get());
    return c;
}

bool find_node(GPNode* n, GPNode* parent, size_t child_idx, size_t& remaining, NodeRef& out) {
    if (!n) return false;
    if (remaining == 0) {
        out.node = n; out.parent = parent; out.child_index = child_idx;
        return true;
    }
    remaining--;
    for (size_t i = 0; i < n->children.size(); ++i)
        if (find_node(n->children[i].get(), n, i, remaining, out)) return true;
    return false;
}

NodeRef select_random_node(GPNode* root, std::mt19937& rng) {
    NodeRef ref;
    if (!root) return ref;
    size_t total = count_nodes(root);
    if (total == 0) return ref;
    size_t target = std::uniform_int_distribution<size_t>(0, total - 1)(rng);
    find_node(root, nullptr, 0, target, ref);
    return ref;
}

void subtree_crossover(Individual& a, Individual& b, std::mt19937& rng) {
    if (!a.tree || !b.tree) return;
    NodeRef ra = select_random_node(a.tree.get(), rng);
    NodeRef rb = select_random_node(b.tree.get(), rng);
    if (!ra.node || !rb.node) return;

    std::unique_ptr<GPNode> tmp = ra.node->clone();
    if (ra.parent) ra.parent->children[ra.child_index] = rb.node->clone();
    else a.tree = rb.node->clone();
    if (rb.parent) rb.parent->children[rb.child_index] = std::move(tmp);
    else b.tree = std::move(tmp);
}

void subtree_mutation(Individual& ind, std::mt19937& rng, int max_depth = 4) {
    if (!ind.tree) return;
    NodeRef r = select_random_node(ind.tree.get(), rng);
    if (!r.node) return;
    auto replacement = random_tree(rng, max_depth);
    if (r.parent) r.parent->children[r.child_index] = std::move(replacement);
    else ind.tree = std::move(replacement);
}

// ---------------------------------------------------------------------------
// Parallel fitness evaluation (DAG fitness stage)
// ---------------------------------------------------------------------------
void evaluate_population_parallel(std::vector<Individual>& population,
                                  const std::vector<double>& xs,
                                  const std::vector<double>& ys) {
    parallel_f::task_list tl;
    for (size_t i = 0; i < population.size(); ++i) {
        // Capture the index, not the range-for reference. That reference
        // dies at the end of each iteration, before task_list::finish() runs.
        auto task = parallel_f::make_task([&population, &xs, &ys, i]() {
            Individual& ind = population[i];
            ind.fitness = ind.evaluate(xs, ys);
        });
        tl.append(task);   // independent → full parallelism
    }
    tl.finish();
    parallel_f::stats::instance::get().show_stats();
}

// ---------------------------------------------------------------------------
// Main — generational loop matching the DAG
// ---------------------------------------------------------------------------
int main() {
    constexpr size_t POP_SIZE    = 128;
    constexpr int    MAX_DEPTH   = 5;
    constexpr size_t GENERATIONS = 30;
    constexpr size_t DATA_POINTS = 50;
    constexpr size_t ELITE       = POP_SIZE / 4;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> u01(0.0, 1.0);

    // Target data: y = x² + sin(x)
    std::vector<double> xs(DATA_POINTS), ys(DATA_POINTS);
    for (size_t i = 0; i < DATA_POINTS; ++i) {
        xs[i] = -3.0 + 6.0 * static_cast<double>(i) / (DATA_POINTS - 1);
        ys[i] = xs[i] * xs[i] + std::sin(xs[i]);
    }

    // Stage 0 — Initialize population
    std::vector<Individual> population;
    population.reserve(POP_SIZE);
    for (size_t i = 0; i < POP_SIZE; ++i)
        population.emplace_back(random_tree(rng, MAX_DEPTH, i % 2 == 0));

    for (size_t gen = 0; gen < GENERATIONS; ++gen) {
        // Stage 1 — Parallel fitness (DAG fitness fan-out)
        evaluate_population_parallel(population, xs, ys);

        // Stage 2 — Selection (depends on fitness)
        std::sort(population.begin(), population.end(),
                  [](const Individual& a, const Individual& b) {
                      return a.fitness < b.fitness;
                  });

        std::cout << "Gen " << gen
                  << "  best MSE: " << population[0].fitness
                  << "  size: " << (population[0].tree ? population[0].tree->size() : 0)
                  << std::endl;

        // Stage 3 — Variation (depends on selection)
        for (size_t i = ELITE; i < POP_SIZE; i += 2) {
            size_t p1 = std::uniform_int_distribution<size_t>(0, ELITE - 1)(rng);
            size_t p2 = std::uniform_int_distribution<size_t>(0, ELITE - 1)(rng);

            population[i] = population[p1];
            if (i + 1 < POP_SIZE)
                population[i + 1] = population[p2];

            if (i + 1 < POP_SIZE && u01(rng) < 0.85)
                subtree_crossover(population[i], population[i + 1], rng);

            if (u01(rng) < 0.25)
                subtree_mutation(population[i], rng, 3);
            if (i + 1 < POP_SIZE && u01(rng) < 0.25)
                subtree_mutation(population[i + 1], rng, 3);
        }
    }

    std::cout << "\nFinal best MSE: " << population[0].fitness << std::endl;
    return 0;
}