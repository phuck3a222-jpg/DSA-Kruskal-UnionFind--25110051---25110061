#include "UnionFind.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

// Balanced unions create trees of depth log2(V) with union-by-size.
// Build time is excluded. Each trial starts from a fresh identical forest.
int main(int argc, char** argv) {
    try {
        const int vertices = 65536, operations = 1000000, trials = 9;
        std::mt19937 random(42);
        std::uniform_int_distribution<int> pick(0, vertices - 1);
        std::vector<int> queries(operations);
        for (int& vertex : queries) vertex = pick(random);
        std::vector<double> times[2];
        unsigned long long expected = 0;
        for (int trial = -1; trial < trials; ++trial) {
            for (int turn = 0; turn < 2; ++turn) {
                int mode = (trial + 1 + turn) % 2;
                UnionFind sets(vertices, mode != 0);
                for (int step = 1; step < vertices; step *= 2)
                    for (int first = 0; first < vertices; first += 2 * step) sets.unite(first, first + step);
                unsigned long long checksum = 0;
                auto start = std::chrono::steady_clock::now();
                for (int vertex : queries) checksum += static_cast<unsigned>(sets.findRoot(vertex)) + 1;
                double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / operations;
                if (trial == -1 && turn == 0) expected = checksum;
                if (checksum != expected) throw std::runtime_error("Find results differ between modes.");
                if (trial >= 0) times[mode].push_back(ns);
            }
        }
        for (auto& values : times) std::sort(values.begin(), values.end());
        auto output = [&](std::ostream& stream) {
            stream << "vertices,find_operations,seed,trials,off_avg_find_ns,on_avg_find_ns,speedup,checksum\n"
                << vertices << ',' << operations << ",42," << trials << ',' << std::fixed << std::setprecision(3)
                << times[0][trials / 2] << ',' << times[1][trials / 2] << ','
                << times[0][trials / 2] / times[1][trials / 2] << ',' << expected << '\n';
        };
        output(std::cout);
        if (argc > 1) {
            std::ofstream file(argv[1]); if (!file) throw std::runtime_error("Cannot create benchmark CSV.");
            output(file); if (!file) throw std::runtime_error("Cannot finish benchmark CSV.");
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
