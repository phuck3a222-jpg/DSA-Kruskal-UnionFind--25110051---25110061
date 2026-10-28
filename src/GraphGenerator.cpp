#include "GraphGenerator.h"
#include "GraphInput.h"
#include <algorithm>
#include <numeric>
#include <ostream>
#include <random>
#include <set>
#include <stdexcept>

Graph generateGraph(const GeneratorOptions& options) {
    const int n = options.vertices;
    const long long maximum = static_cast<long long>(n) * (n - 1) / 2;
    if (n < 0 || n > GraphInput::MAX_VERTICES || options.edges < 0 ||
        options.edges > GraphInput::MAX_EDGES || options.edges > maximum ||
        options.minWeight > options.maxWeight ||
        (options.connected && (n == 0 || options.edges < n - 1))) {
        throw std::invalid_argument("Invalid generator settings. Simple graph: E <= V*(V-1)/2; connected: V >= 1 and E >= V-1.");
    }
    Graph graph;
    graph.vertexCount = n;
    graph.edges.reserve(options.edges);
    std::mt19937 random(options.seed);
    std::uniform_int_distribution<int> weight(options.minWeight, options.maxWeight);
    std::set<std::pair<int, int>> pairs;
    auto addEdge = [&](int a, int b) {
        if (a == b) return;
        if (a > b) std::swap(a, b);
        if (pairs.insert({a, b}).second) graph.edges.push_back({a, b, weight(random)});
    };
    if (options.connected) {
        // A random spanning tree first guarantees connectivity.
        std::vector<int> order(n);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), random);
        for (int i = 1; i < n; ++i) {
            std::uniform_int_distribution<int> parent(0, i - 1);
            addEdge(order[i], order[parent(random)]);
        }
    }
    if (options.edges > maximum / 2) {
        // Dense small graphs: enumerate unused pairs instead of retrying duplicates.
        std::vector<std::pair<int, int>> remaining;
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b)
                if (!pairs.count({a, b})) remaining.push_back({a, b});
        std::shuffle(remaining.begin(), remaining.end(), random);
        for (const auto& pair : remaining) {
            if (graph.edges.size() == static_cast<std::size_t>(options.edges)) break;
            addEdge(pair.first, pair.second);
        }
    } else if (n > 1) {
        std::uniform_int_distribution<int> vertex(0, n - 1);
        while (graph.edges.size() < static_cast<std::size_t>(options.edges)) {
            const int a = vertex(random);
            const int b = vertex(random);
            addEdge(a, b);
        }
    }
    return graph;
}

void writeGraph(std::ostream& output, const Graph& graph) {
    GraphInput::validate(graph);
    output << graph.vertexCount << ' ' << graph.edges.size() << '\n';
    for (const Edge& edge : graph.edges)
        output << edge.from << ' ' << edge.to << ' ' << edge.weight << '\n';
    if (!output) throw std::runtime_error("Could not write graph file.");
}
