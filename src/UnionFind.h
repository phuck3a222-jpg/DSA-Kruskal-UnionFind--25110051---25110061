#ifndef UNION_FIND_H
#define UNION_FIND_H

#include <vector>

class UnionFind {
private:
    std::vector<int> parent;
    std::vector<int> setSize;
    int componentCount;
    bool useCompression;

public:
    UnionFind(int vertexCount, bool compressionEnabled = true);
    int findRoot(int vertex);
    bool unite(int firstVertex, int secondVertex);
    int getComponentCount() const;
    int getParent(int vertex) const;
};

#endif
