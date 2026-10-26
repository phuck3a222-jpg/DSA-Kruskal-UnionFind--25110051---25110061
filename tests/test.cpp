#include "Kruskal.h"
#include "UnionFind.h"
#include <iostream>
#include <string>
using namespace std;

Graph createExample() {
    Graph graph;
    graph.vertexCount = 4;
    graph.edges = {{0, 1, 1}, {1, 2, 2}, {0, 2, 3}, {2, 3, 4}, {1, 3, 5}};
    return graph;
}

bool checkUnionFind(bool useCompression) {
    UnionFind groups(8, useCompression);
    groups.unite(0, 1);
    groups.unite(2, 3);
    groups.unite(0, 2);
    groups.unite(4, 5);
    groups.unite(6, 7);
    groups.unite(4, 6);
    groups.unite(0, 4);
    if (groups.getParent(7) != 6 || groups.findRoot(7) != 0) {
        return false;
    }
    if (useCompression && (groups.getParent(7) != 0 || groups.getParent(6) != 0)) {
        return false;
    }
    if (!useCompression && (groups.getParent(7) != 6 || groups.getParent(6) != 4)) {
        return false;
    }
    return !groups.unite(7, 0) && groups.getComponentCount() == 1;
}

bool checkExample(bool useCompression) {
    Kruskal solver;
    MSTResult result = solver.solve(createExample(), useCompression);
    if (!result.hasSpanningTree || result.totalWeight != 7 || result.edges.size() != 3) {
        return false;
    }
    return result.edges[0].from == 0 && result.edges[0].to == 1 &&
           result.edges[1].from == 1 && result.edges[1].to == 2 &&
           result.edges[2].from == 2 && result.edges[2].to == 3;
}

bool reportCheck(const string& name, bool passed) {
    if (passed) {
        cout << "PASS: " << name << '\n';
    } else {
        cout << "FAIL: " << name << '\n';
    }
    return passed;
}

int runTests() {
    int passed = 0;
    passed += reportCheck("Union-Find without compression", checkUnionFind(false));
    passed += reportCheck("Union-Find with compression", checkUnionFind(true));
    passed += reportCheck("Hand-traced MST without compression", checkExample(false));
    passed += reportCheck("Hand-traced MST with compression", checkExample(true));
    Kruskal solver;
    Graph singleton;
    singleton.vertexCount = 1;
    MSTResult singleResult = solver.solve(singleton, true);
    passed += reportCheck("Single vertex", singleResult.hasSpanningTree &&
        singleResult.edges.empty() && singleResult.totalWeight == 0);
    Graph empty;
    MSTResult emptyResult = solver.solve(empty, true);
    passed += reportCheck("Empty graph", !emptyResult.hasSpanningTree && emptyResult.edges.empty());
    Graph disconnected;
    disconnected.vertexCount = 3;
    disconnected.edges = {{0, 1, 2}};
    MSTResult forest = solver.solve(disconnected, true);
    passed += reportCheck("Disconnected graph", !forest.hasSpanningTree &&
        forest.componentCount == 2 && forest.totalWeight == 2);
    UnionFind groups(3, true);
    groups.unite(0, 1);
    groups.unite(2, 0);
    passed += reportCheck("Attach smaller group to larger group",
        groups.findRoot(2) == groups.findRoot(1) && groups.getComponentCount() == 1);
    cout << "RESULT: " << passed << "/8 checks passed.\n";
    if (passed == 8) {
        return 0;
    }
    return 1;
}

int main() {
    return runTests();
}
