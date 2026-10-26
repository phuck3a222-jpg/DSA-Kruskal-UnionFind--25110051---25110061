#include "GraphInput.h"
#include <istream>
#include <stdexcept>
#include <string>

namespace GraphInput {
void validate(const Graph& graph) {
    if (graph.vertexCount < 0 || graph.vertexCount > MAX_VERTICES ||
        graph.edges.size() > static_cast<std::size_t>(MAX_EDGES)) {
        throw std::invalid_argument("Counts exceed supported limits (V: 0..1000000, E: 0..2000000).");
    }
    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const Edge& edge = graph.edges[i];
        if (edge.from < 0 || edge.from >= graph.vertexCount ||
            edge.to < 0 || edge.to >= graph.vertexCount) {
            throw std::invalid_argument("Invalid vertex index for edge " + std::to_string(i + 1) + ". Use 0..V-1.");
        }
    }
}

Graph read(std::istream& input) {
    Graph graph;
    int edgeCount;
    if (!(input >> graph.vertexCount >> edgeCount)) {
        throw std::invalid_argument("V and E must be integers.");
    }
    if (edgeCount < 0 || edgeCount > MAX_EDGES) {
        throw std::invalid_argument("E must be between 0 and 2000000.");
    }
    validate(graph);
    for (int i = 0; i < edgeCount; ++i) {
        Edge edge;
        if (!(input >> edge.from >> edge.to >> edge.weight)) {
            throw std::invalid_argument("Invalid or missing data for edge " + std::to_string(i + 1) + ". Expected: from to weight (integers).");
        }
        graph.edges.push_back(edge);
    }
    input >> std::ws;
    if (!input.eof()) {
        throw std::invalid_argument("Unexpected data after the declared edges. Check E and the input format.");
    }
    validate(graph);
    return graph;
}
}
