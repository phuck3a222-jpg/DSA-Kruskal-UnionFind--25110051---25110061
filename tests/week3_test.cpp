#include "GraphGenerator.h"
#include "GraphInput.h"
#include "Kruskal.h"
#include <climits>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

void check(bool condition, const char* label) {
    if (!condition) throw std::runtime_error(label);
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Invalid generator/sorted input accepted");
}
int main() {
    try {
        int count = 0;
        for (int n = 1; n <= 15; ++n) {
            for (int density = 0; density < 3; ++density) {
                GeneratorOptions options;
                options.vertices = n;
                int maxEdges = n * (n - 1) / 2;
                options.edges = density == 0 ? n - 1 : density == 1 ? (maxEdges + n - 1) / 2 : maxEdges;
                options.minWeight = -8; options.maxWeight = 8; options.seed = 21;
                Graph first = generateGraph(options), again = generateGraph(options);
                check(first.edges.size() == static_cast<std::size_t>(options.edges), "Wrong generated edge count");
                std::set<std::pair<int, int>> pairs;
                for (std::size_t i = 0; i < first.edges.size(); ++i) {
                    const Edge& edge = first.edges[i];
                    check(edge.from < edge.to && pairs.insert({edge.from, edge.to}).second, "Loop or duplicate");
                    check(edge.weight >= -8 && edge.weight <= 8, "Weight out of range");
                    check(edge.from == again.edges[i].from && edge.to == again.edges[i].to && edge.weight == again.edges[i].weight, "Seed not reproducible");
                }
                MSTResult result = Kruskal().solve(first);
                check(result.hasSpanningTree && result.componentCount == 1, "Connected generator disconnected");
                Kruskal::sortEdges(first);
                for (bool compression : {false, true}) {
                    MSTResult sorted = Kruskal().solveSorted(first, compression);
                    check(sorted.totalWeight == result.totalWeight && sorted.edges.size() == result.edges.size(), "Sorted pass mismatch");
                }
                std::stringstream file;
                writeGraph(file, first);
                Graph parsed = GraphInput::read(file);
                check(Kruskal().solve(parsed).totalWeight == result.totalWeight, "File round trip");
                ++count;
            }
        }
        GeneratorOptions options;
        options.vertices = 0; options.edges = 0; options.connected = false;
        check(generateGraph(options).edges.empty(), "Empty graph");
        options.connected = true; rejects([&] { generateGraph(options); });
        options.vertices = 3; options.edges = 1; rejects([&] { generateGraph(options); });
        options.edges = 4; rejects([&] { generateGraph(options); });
        options.edges = 2; options.minWeight = 5; options.maxWeight = 1;
        rejects([&] { generateGraph(options); });
        options.minWeight = INT_MIN; options.maxWeight = INT_MAX;
        check(generateGraph(options).edges.size() == 2, "Full int weight range");
        options.connected = false; options.vertices = 5; options.edges = 0;
        check(Kruskal().solve(generateGraph(options)).componentCount == 5, "Isolated generator vertices");
        Graph unsorted; unsorted.vertexCount = 3; unsorted.edges = {{0, 1, 2}, {1, 2, 1}};
        rejects([&] { Kruskal().solveSorted(unsorted); });
        std::cout << "PASS: " << count << " generator scenarios, round trips, sorted pass and boundary cases.\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
