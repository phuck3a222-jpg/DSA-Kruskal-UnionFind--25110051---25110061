#include "UnionFind.h"
#include <stdexcept>
using namespace std;

UnionFind::UnionFind(int vertexCount, bool compressionEnabled) {
    if (vertexCount < 0) {
        throw invalid_argument("Vertex count cannot be negative.");
    }
    parent.resize(vertexCount);
    setSize.resize(vertexCount);
    componentCount = vertexCount;
    useCompression = compressionEnabled;

    for (int vertex = 0; vertex < vertexCount; ++vertex) {
        parent[vertex] = vertex;
        setSize[vertex] = 1;
    }
}

int UnionFind::findRoot(int vertex) {
    if (vertex < 0 || vertex >= static_cast<int>(parent.size())) {
        throw out_of_range("Union-Find vertex index out of range.");
    }
    int root = vertex;
    while (parent[root] != root) {
        root = parent[root];
    }
    if (useCompression) {
        while (parent[vertex] != vertex) {
            int nextVertex = parent[vertex];
            parent[vertex] = root;
            vertex = nextVertex;
        }
    }
    return root;
}

bool UnionFind::unite(int firstVertex, int secondVertex) {
    // Validate both arguments before path compression can change either path.
    if (firstVertex < 0 || secondVertex < 0 ||
        firstVertex >= static_cast<int>(parent.size()) ||
        secondVertex >= static_cast<int>(parent.size())) {
        throw out_of_range("Union-Find vertex index out of range.");
    }
    int firstRoot = findRoot(firstVertex);
    int secondRoot = findRoot(secondVertex);
    if (firstRoot == secondRoot) {
        return false;
    }
    if (setSize[firstRoot] < setSize[secondRoot]) {
        int temporary = firstRoot;
        firstRoot = secondRoot;
        secondRoot = temporary;
    }
    parent[secondRoot] = firstRoot;
    setSize[firstRoot] += setSize[secondRoot];
    --componentCount;
    return true;
}

int UnionFind::getComponentCount() const {
    return componentCount;
}

int UnionFind::getParent(int vertex) const {
    return parent.at(vertex);
}
