#ifndef GRAPH_GENERATOR_H
#define GRAPH_GENERATOR_H

#include "Graph.h"
#include <iosfwd>

struct GeneratorOptions {
    int vertices = 6;
    int edges = 9;
    int minWeight = 1;
    int maxWeight = 20;
    unsigned seed = 42;
    bool connected = true;
};

// Generates a simple undirected graph: no loops or duplicate vertex pairs.
Graph generateGraph(const GeneratorOptions& options);
void writeGraph(std::ostream& output, const Graph& graph);

#endif
