#include "Kruskal.h"
#include "GraphInput.h"
#include <iostream>
#include <exception>
using namespace std;

void printResult(const Graph& graph, const MSTResult& result) {
    if (graph.vertexCount == 0) {
        cout << "Empty graph: no spanning tree.\n";
    } else if (!result.hasSpanningTree) {
        cout << "Disconnected graph: no spanning tree.\n";
        cout << "Minimum spanning forest:\n";
    } else {
        cout << "Minimum spanning tree:\n";
    }
    int edgeCount = result.edges.size();
    for (int index = 0; index < edgeCount; ++index) {
        Edge edge = result.edges[index];
        cout << edge.from << " - " << edge.to << " | weight = " << edge.weight << '\n';
    }
    cout << "Selected edges: " << result.edges.size() << '\n';
    cout << "Total weight: " << result.totalWeight << '\n';
    cout << "Components: " << result.componentCount << '\n';
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout << "Enter V E, followed by E triples: from to weight (vertices: 0..V-1).\n"
         << "Finish input with EOF (Windows: Ctrl+Z then Enter).\n" << flush;
    try {
        Graph graph = GraphInput::read(cin);
        Kruskal solver;
        printResult(graph, solver.solve(graph));
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
