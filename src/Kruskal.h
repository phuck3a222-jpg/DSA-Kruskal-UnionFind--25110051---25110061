#ifndef KRUSKAL_H
#define KRUSKAL_H

#include "Graph.h"
#include "MSTResult.h"

class Kruskal {
public:
    MSTResult solve(const Graph& graph, bool useCompression = true);
};

#endif
