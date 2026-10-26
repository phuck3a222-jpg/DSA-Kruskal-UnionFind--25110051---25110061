#ifndef MST_RESULT_H
#define MST_RESULT_H

#include <vector>
#include "Edge.h"

struct MSTResult {
    std::vector<Edge> edges;
    long long totalWeight = 0;
    int componentCount = 0;
    bool hasSpanningTree = false;
};

#endif
