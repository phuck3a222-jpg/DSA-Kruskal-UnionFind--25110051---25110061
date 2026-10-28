#ifndef KRUSKAL_H
#define KRUSKAL_H

#include "Graph.h"
#include "MSTResult.h"

class Kruskal {
public:
    MSTResult solve(const Graph& graph, bool useCompression = true);
    // Input edges must already follow Kruskal's weight/from/to ordering.
    // Used to measure the Union-Find pass separately from sorting.
    MSTResult solveSorted(const Graph& sortedGraph, bool useCompression = true);
    static void sortEdges(Graph& graph);
};

#endif
