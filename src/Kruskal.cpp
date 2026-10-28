#include "Kruskal.h"
#include "UnionFind.h"
#include "GraphInput.h"
#include <algorithm>
#include <stdexcept>
using namespace std;

bool compareEdges(const Edge& first, const Edge& second) {
    if (first.weight != second.weight) {
        return first.weight < second.weight;
    }
    if (first.from != second.from) {
        return first.from < second.from;
    }
    return first.to < second.to;
}

MSTResult Kruskal::solve(const Graph& graph, bool useCompression) {
    GraphInput::validate(graph);
    Graph sortedGraph = graph;
    sortEdges(sortedGraph);
    return solveSorted(sortedGraph, useCompression);
}

void Kruskal::sortEdges(Graph& graph) {
    sort(graph.edges.begin(), graph.edges.end(), compareEdges);
}

MSTResult Kruskal::solveSorted(const Graph& graph, bool useCompression) {
    GraphInput::validate(graph);
    if (!is_sorted(graph.edges.begin(), graph.edges.end(), compareEdges)) {
        throw std::invalid_argument("solveSorted requires sorted edges.");
    }
    MSTResult result;
    result.componentCount = graph.vertexCount;
    if (graph.vertexCount == 0) {
        return result;
    }
    UnionFind groups(graph.vertexCount, useCompression);
    int edgeCount = graph.edges.size();
    int selectedEdgeCount = 0;

    for (int index = 0; index < edgeCount; ++index) {
        Edge edge = graph.edges[index];
        if (groups.unite(edge.from, edge.to)) {
            result.edges.push_back(edge);
            result.totalWeight += edge.weight;
            ++selectedEdgeCount;

            if (selectedEdgeCount == graph.vertexCount - 1) {
                break;
            }
        }
    }
    result.componentCount = groups.getComponentCount();
    result.hasSpanningTree = (result.componentCount == 1);
    return result;
}
