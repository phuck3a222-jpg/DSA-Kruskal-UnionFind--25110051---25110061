#include "Kruskal.h"
#include "UnionFind.h"
#include "GraphInput.h"
#include <algorithm>
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
    MSTResult result;
    result.componentCount = graph.vertexCount;
    if (graph.vertexCount == 0) {
        return result;
    }
    vector<Edge> sortedEdges = graph.edges;
    sort(sortedEdges.begin(), sortedEdges.end(), compareEdges);
    UnionFind groups(graph.vertexCount, useCompression);
    int edgeCount = sortedEdges.size();
    int selectedEdgeCount = 0;

    for (int index = 0; index < edgeCount; ++index) {
        Edge edge = sortedEdges[index];
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
