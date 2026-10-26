#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include "Edge.h"

struct Graph {
    int vertexCount = 0;
    std::vector<Edge> edges;
};

#endif
