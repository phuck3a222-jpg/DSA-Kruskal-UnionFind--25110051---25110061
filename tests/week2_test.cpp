#include "GraphInput.h"
#include "Kruskal.h"
#include "UnionFind.h"
#include <climits>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

int checks = 0;
void check(bool condition, const std::string& name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

template<class Error, class Action>
void rejects(Action action, const std::string& name) {
    bool caught = false;
    try { action(); } catch (const Error&) { caught = true; }
    check(caught, name);
}

Graph parse(const std::string& text) {
    std::istringstream input(text);
    return GraphInput::read(input);
}

// Independent O(V^2 + VE) Prim forest oracle, used only on small graphs.
MSTResult prim(const Graph& graph) {
    MSTResult result;
    std::vector<bool> used(graph.vertexCount, false);
    std::vector<long long> best(graph.vertexCount, LLONG_MAX);
    for (int step = 0; step < graph.vertexCount; ++step) {
        int vertex = -1;
        for (int v = 0; v < graph.vertexCount; ++v)
            if (!used[v] && (vertex == -1 || best[v] < best[vertex])) vertex = v;
        if (best[vertex] == LLONG_MAX) ++result.componentCount;
        else result.totalWeight += best[vertex];
        used[vertex] = true;
        for (const Edge& edge : graph.edges) {
            int other = edge.from == vertex ? edge.to : edge.to == vertex ? edge.from : -1;
            if (other >= 0 && !used[other] && edge.weight < best[other]) best[other] = edge.weight;
        }
    }
    return result;
}

int main() {
    try {
        const char* invalid[] = {
            "", "x 0", "-1 0", "1000001 0", "2 -1", "2 2000001",
            "2 1", "2 1 0 1", "2 1 0 1 x", "2 1 -1 1 3", "2 1 0 2 3",
            "0 1 0 0 1", "2 0 0 1 3", "2 1 0 1 1.5", "2 1 0 1 2147483648",
            "2147483648 0", "2 0 trailing"
        };
        for (const char* text : invalid)
            rejects<std::invalid_argument>([&] { parse(text); }, std::string("Reject: ") + text);

        check(parse("0 0\n \t").edges.empty(), "Empty graph and trailing whitespace");
        for (bool compression : {false, true}) {
            Kruskal solver;
            MSTResult singleton = solver.solve(parse("1 1 0 0 -100"), compression);
            check(singleton.hasSpanningTree && singleton.totalWeight == 0 && singleton.edges.empty(), "Singleton self-loop");
            MSTResult forest = solver.solve(parse("5 2 0 1 -2 2 3 4"), compression);
            check(!forest.hasSpanningTree && forest.componentCount == 3 && forest.totalWeight == 2, "Forest with isolated vertex");
            MSTResult special = solver.solve(parse("3 6 0 0 -100 0 1 5 0 1 -3 1 2 -2 0 2 10 1 1 -99"), compression);
            check(special.hasSpanningTree && special.totalWeight == -5 && special.edges.size() == 2, "Parallel edges, loops and negatives");
            MSTResult tied = solver.solve(parse("3 3 0 1 1 1 2 1 0 2 1"), compression);
            check(tied.totalWeight == 2 && tied.edges.size() == 2, "Equal weights");
            MSTResult large = solver.solve(parse("3 2 0 1 2147483647 1 2 2147483647"), compression);
            check(large.totalWeight == 4294967294LL, "64-bit total weight");
        }
        Graph bad;
        bad.vertexCount = -1;
        rejects<std::invalid_argument>([&] { Kruskal().solve(bad); }, "Direct API negative V");
        bad.vertexCount = 2;
        bad.edges = {{0, 2, 1}, {0, 1, 0}};
        rejects<std::invalid_argument>([&] { Kruskal().solve(bad); }, "Validate all edges before early exit");
        rejects<std::invalid_argument>([] { UnionFind invalid(-1); }, "Negative Union-Find size");
        UnionFind groups(2);
        rejects<std::out_of_range>([&] { groups.findRoot(-1); }, "Negative find index");
        rejects<std::out_of_range>([&] { groups.unite(0, 2); }, "Invalid union index");
        rejects<std::out_of_range>([&] { groups.getParent(2); }, "Invalid parent index");
        check(groups.getComponentCount() == 2, "Invalid union preserves components");

        std::mt19937 random(25110051);
        for (int trial = 0; trial < 250; ++trial) {
            Graph graph;
            graph.vertexCount = 1 + random() % 9;
            int count = random() % 35;
            for (int i = 0; i < count; ++i) graph.edges.push_back({
                static_cast<int>(random() % graph.vertexCount),
                static_cast<int>(random() % graph.vertexCount), static_cast<int>(random() % 41) - 20});
            MSTResult expected = prim(graph);
            for (bool compression : {false, true}) {
                MSTResult actual = Kruskal().solve(graph, compression);
                check(actual.totalWeight == expected.totalWeight && actual.componentCount == expected.componentCount &&
                    actual.edges.size() == static_cast<std::size_t>(graph.vertexCount - expected.componentCount) &&
                    actual.hasSpanningTree == (expected.componentCount == 1), "Random graph vs Prim");
                UnionFind selected(graph.vertexCount);
                long long sum = 0;
                for (const Edge& edge : actual.edges) {
                    check(selected.unite(edge.from, edge.to), "Selected edges have no cycles");
                    sum += edge.weight;
                }
                check(sum == actual.totalWeight, "Selected edges match reported weight");
            }
        }
        std::cout << "PASS: " << checks << " checks (250 random graphs, both compression modes).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
