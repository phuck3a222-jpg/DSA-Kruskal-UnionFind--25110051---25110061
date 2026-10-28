#ifndef GRAPH_VIEW_H
#define GRAPH_VIEW_H

#include <windows.h>
#include "Graph.h"
#include "MSTResult.h"
#include <set>
#include "VertexLabels.h"

namespace GuiLimits {
    const int MAX_VERTICES = 1000000;
    const int MAX_EDGES = 2000000;
    const int DRAW_VERTICES = 1000;
    const int DRAW_EDGES = 3000;
    const int MAX_PARALLEL = 3;
    void validate(const Graph& graph);
}

struct GraphPoint {
    double x, y;
    GraphPoint(double xValue = 0, double yValue = 0) : x(xValue), y(yValue) {}
};
enum class GraphLayout { Circle, Grid };

// All coordinates are logical 96-DPI units. Positions are relative to the canvas.
// Zoom/pan affect only drawing; they never change Graph or run Kruskal.
struct GraphViewState {
    VertexLabels labels;
    std::vector<GraphPoint> positions;
    std::set<int> selectedVertices, selectedEdges;
    double zoom = 1.0;
    GraphPoint pan;
    bool selectingRectangle = false;
    RECT selectionRectangle = {};
    int connectionStart = -1;
    POINT cursor = {};
    int hoveredVertex = -1, hoveredEdge = -1;
    bool showAllWeights = true;
    std::vector<int> visibleEdges;
    std::set<int> mstSourceEdges;
    bool hasMstCache = false;
    double vertexRadius = 24;
    int hoveredCluster = -1;
};
struct GraphCluster {
    std::vector<int> vertices;
    GraphPoint center;
};
struct GraphHit {
    int vertex = -1, edge = -1;
    bool edgeLabel = false;
    int cluster = -1;
};
bool edgeLabelVisible(const Graph& graph, const GraphViewState& view, int edge);
double graphVertexRadius(const GraphViewState& view);
std::vector<GraphCluster> graphClusters(const GraphViewState& view, const RECT& bounds);
void updateVisibleEdges(GraphViewState& view, const Graph& graph);
void cacheMstEdges(GraphViewState& view, const Graph& graph, const MSTResult& result);

void layoutGraph(GraphViewState& view, const Graph& graph, const RECT& bounds, GraphLayout layout);
void fitGraph(GraphViewState& view, const Graph& graph, const RECT& bounds);
GraphPoint graphToScreen(const GraphViewState& view, const RECT& bounds, GraphPoint point);
GraphPoint screenToGraph(const GraphViewState& view, const RECT& bounds, GraphPoint point);
void zoomGraph(GraphViewState& view, const RECT& bounds, double factor, GraphPoint anchor);
GraphHit hitTestGraph(const Graph& graph, const GraphViewState& view, const RECT& bounds, POINT point);
RECT graphEdgeLabel(const Graph& graph, const GraphViewState& view, const RECT& bounds, int edge);
std::vector<GraphPoint> graphEdgePath(const Graph& graph, const GraphViewState& view, const RECT& bounds, int edge);
void selectGraphRectangle(const Graph& graph, GraphViewState& view, const RECT& bounds, RECT rectangle);
void drawGraphInteractive(HDC dc, const RECT& bounds, const Graph& graph,
                          const MSTResult* result, HFONT font, const GraphViewState& view);

// Small editor operations live outside dsa_core and can be tested independently.
namespace GraphEditing {
    void addVertex(Graph& graph, GraphViewState& view, const RECT& bounds, const GraphPoint* at = nullptr);
    void addEdge(Graph& graph, int from, int to, int weight);
    bool deleteSelected(Graph& graph, GraphViewState& view);
}

// A circular layout and GDI drawing only; no algorithm or input handling here.
void drawGraph(HDC dc, const RECT& bounds, const Graph& graph,
               const MSTResult* result, HFONT font);

#endif
