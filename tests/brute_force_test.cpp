#include "Kruskal.h"
#include "GraphInput.h"
#include <climits>
#include <iostream>
#include <random>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <fstream>

struct BruteResult {
    long long weight = LLONG_MAX;
    unsigned candidates = 0, spanningTrees = 0;
    std::vector<Edge> edges;
};

void enumerate(const Graph& graph, int next, std::vector<int>& selected, BruteResult& result) {
    int needed = graph.vertexCount - 1 - static_cast<int>(selected.size());
    if (needed == 0) {
        ++result.candidates;
        std::vector<std::vector<int>> adjacent(graph.vertexCount);
        long long weight = 0;
        for (int index : selected) {
            const Edge& edge = graph.edges[index]; weight += edge.weight;
            adjacent[edge.from].push_back(edge.to); adjacent[edge.to].push_back(edge.from);
        }
        // Independent oracle: connected + V-1 edges implies a tree.
        // Do not reuse the Union-Find implementation being validated.
        std::vector<bool> seen(graph.vertexCount, false); std::vector<int> stack(1, 0); seen[0] = true;
        int visited = 0;
        while (!stack.empty()) {
            int vertex = stack.back(); stack.pop_back(); ++visited;
            for (int next : adjacent[vertex]) if (!seen[next]) { seen[next] = true; stack.push_back(next); }
        }
        if (visited == graph.vertexCount) {
            ++result.spanningTrees;
            if (weight < result.weight) {
                result.weight = weight; result.edges.clear();
                for (int index : selected) result.edges.push_back(graph.edges[index]);
            }
        }
        return;
    }
    for (int i = next; i + needed <= static_cast<int>(graph.edges.size()); ++i) {
        selected.push_back(i); enumerate(graph, i + 1, selected, result); selected.pop_back();
    }
}

BruteResult bruteForce(const Graph& graph) {
    if (graph.vertexCount < 1 || graph.vertexCount > 8 || graph.edges.size() > 16)
        throw std::invalid_argument("Exhaustive checker is limited to 1..8 vertices and 16 edges.");
    GraphInput::validate(graph);
    BruteResult result; std::vector<int> selected;
    enumerate(graph, 0, selected, result); return result;
}

void compare(const Graph& graph, long long expected) {
    for (bool compression : {false, true}) {
        MSTResult actual = Kruskal().solve(graph, compression);
        if (!actual.hasSpanningTree || actual.totalWeight != expected || actual.edges.size() != static_cast<std::size_t>(graph.vertexCount - 1))
            throw std::runtime_error("Kruskal disagrees with exhaustive combination enumeration.");
    }
}

int main(int argc, char** argv) {
    try {
        std::string sourceFile = __FILE__;
        std::string examples = argc > 1 ? argv[1] : sourceFile.substr(0, sourceFile.find_last_of("/\\")) + "/../examples";
        const long long weights[] = {11, 12, 14};
        const unsigned combinations[] = {56, 210, 792};
        for (int vertices = 6; vertices <= 8; ++vertices) {
            std::string name = "brute_force_" + std::to_string(vertices) + ".txt";
            std::ifstream input(examples + "/" + name);
            if (!input) throw std::runtime_error("Cannot open fixture: " + name);
            Graph graph = GraphInput::read(input);
            BruteResult result = bruteForce(graph);
            if (graph.vertexCount != vertices || result.candidates != combinations[vertices - 6] || result.weight != weights[vertices - 6])
                throw std::runtime_error("Fixed fixture differs from documented exhaustive result.");
            compare(graph, result.weight);
            std::cout << "PASS: " << name << ", V=" << graph.vertexCount << ", E=" << graph.edges.size()
                << ", combinations=" << result.candidates << ", spanning_trees=" << result.spanningTrees
                << ", brute_weight=" << result.weight << ", Kruskal_off=" << result.weight << ", Kruskal_on=" << result.weight << '\n';
            std::cout << "One minimum tree:";
            for (const Edge& edge : result.edges) std::cout << ' ' << edge.from << '-' << edge.to << '(' << edge.weight << ')';
            std::cout << '\n';
        }
        std::mt19937 random(42);
        for (int trial = 0; trial < 100; ++trial) {
            Graph graph; graph.vertexCount = 2 + random() % 5;
            for (int i = 1; i < graph.vertexCount; ++i) graph.edges.push_back({i - 1, i, static_cast<int>(random() % 21) - 10});
            while (graph.edges.size() < 10)
                graph.edges.push_back({static_cast<int>(random() % graph.vertexCount), static_cast<int>(random() % graph.vertexCount), static_cast<int>(random() % 21) - 10});
            compare(graph, bruteForce(graph).weight);
        }
        std::cout << "PASS: 3 fixed fixtures (6..8 vertices) and 100 random graphs, both compression modes.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
