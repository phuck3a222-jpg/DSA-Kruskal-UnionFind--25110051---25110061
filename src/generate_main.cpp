#include "GraphGenerator.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

long long integer(const char* value) {
    std::string text(value);
    std::size_t used = 0;
    long long result = std::stoll(text, &used);
    if (used != text.size()) throw std::invalid_argument("Arguments must be integers.");
    return result;
}

int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr << "Usage: DSA_generate V E minWeight maxWeight seed connected(0|1) output.txt\n"
                     "Example: DSA_generate 6 9 1 20 42 1 graph.txt\n";
        return 1;
    }
    try {
        GeneratorOptions options;
        int* fields[] = {&options.vertices, &options.edges, &options.minWeight, &options.maxWeight};
        for (int i = 0; i < 4; ++i) {
            long long value = integer(argv[i + 1]);
            if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
                throw std::invalid_argument("Argument outside int range.");
            *fields[i] = static_cast<int>(value);
        }
        long long seed = integer(argv[5]);
        long long connected = integer(argv[6]);
        if (seed < 0 || seed > std::numeric_limits<unsigned>::max() || (connected != 0 && connected != 1))
            throw std::invalid_argument("Invalid seed or connected flag.");
        options.seed = static_cast<unsigned>(seed);
        options.connected = connected == 1;
        Graph graph = generateGraph(options);
        std::ofstream file(argv[7]);
        writeGraph(file, graph);
        std::cout << "Saved " << graph.vertexCount << " vertices, " << graph.edges.size() << " edges.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
