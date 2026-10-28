#include "GraphGenerator.h"
#include "Kruskal.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("Usage: DSA_benchmark [output.csv]");
        std::ofstream file;
        if (argc == 2) {
            file.open(argv[1]);
            if (!file) throw std::runtime_error("Cannot open output CSV.");
        }
        const char* header = "vertices,edges,seed,repeats,sort_ms,pass_off_ms,pass_on_ms,total_off_ms,total_on_ms,pass_speedup,total_weight,components\n";
        std::cout << header;
        if (file) file << header;
        const int sizes[][2] = {{1000, 5000}, {5000, 25000}, {10000, 50000}, {20000, 100000}, {20000, 300000}};
        for (const auto& size : sizes) {
            GeneratorOptions options;
            options.vertices = size[0]; options.edges = size[1];
            options.minWeight = -1000; options.maxWeight = 1000; options.seed = 42;
            const Graph original = generateGraph(options);
            std::vector<double> sorting, passes[2], totals[2];
            MSTResult reference = Kruskal().solve(original);
            // Warm-up, then nine measured trials. Alternate mode order to reduce bias.
            for (int trial = -1; trial < 9; ++trial) {
                Graph sorted = original; // Input copy and generation are outside sorting measurement.
                auto start = Clock::now();
                Kruskal::sortEdges(sorted);
                double sortTime = elapsed(start);
                if (trial >= 0) sorting.push_back(sortTime);
                for (int turn = 0; turn < 2; ++turn) {
                    int mode = (trial + 1 + turn) % 2;
                    start = Clock::now();
                    MSTResult passResult = Kruskal().solveSorted(sorted, mode != 0);
                    double passTime = elapsed(start);
                    start = Clock::now();
                    MSTResult fullResult = Kruskal().solve(original, mode != 0);
                    double fullTime = elapsed(start);
                    if (passResult.totalWeight != reference.totalWeight || fullResult.totalWeight != reference.totalWeight ||
                        passResult.componentCount != reference.componentCount || fullResult.componentCount != reference.componentCount ||
                        passResult.edges.size() != reference.edges.size() || fullResult.edges.size() != reference.edges.size())
                        throw std::runtime_error("Compression modes produced inconsistent results.");
                    if (trial >= 0) { passes[mode].push_back(passTime); totals[mode].push_back(fullTime); }
                }
            }
            auto row = [&](std::ostream& out) {
                out << std::fixed << std::setprecision(6) << options.vertices << ',' << options.edges << ",42,9,"
                    << median(sorting) << ',' << median(passes[0]) << ',' << median(passes[1]) << ','
                    << median(totals[0]) << ',' << median(totals[1]) << ',' << median(passes[0]) / median(passes[1])
                    << ',' << reference.totalWeight << ',' << reference.componentCount << '\n';
            };
            row(std::cout);
            if (file) row(file);
        }
        if (file.is_open() && !file) throw std::runtime_error("Could not finish writing CSV.");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
