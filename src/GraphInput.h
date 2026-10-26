#ifndef GRAPH_INPUT_H
#define GRAPH_INPUT_H

#include "Graph.h"
#include <iosfwd>

// Shared contract for console, GUI and direct callers. Vertices are zero-based.
namespace GraphInput {
    const int MAX_VERTICES = 1000000;
    const int MAX_EDGES = 2000000;
    // Throws std::invalid_argument on invalid counts or endpoints.
    void validate(const Graph& graph);
    // Reads V E followed by exactly E triples; rejects trailing non-whitespace.
    // Throws std::invalid_argument on malformed input.
    Graph read(std::istream& input);
}

#endif
