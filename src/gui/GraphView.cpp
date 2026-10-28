#define NOMINMAX
#include "GraphView.h"
#include "GraphInput.h"
#include "UnionFind.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
const double PI = 3.141592653589793;
const COLORREF ink = RGB(26, 43, 65), blue = RGB(37, 99, 235), muted = RGB(100, 116, 139);
const COLORREF red = RGB(239, 68, 68);
double distance(GraphPoint a, GraphPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
POINT point(GraphPoint p) { POINT result = {static_cast<LONG>(std::lround(p.x)), static_cast<LONG>(std::lround(p.y))}; return result; }
RECT normalized(RECT r) {
    if (r.left > r.right) std::swap(r.left, r.right);
    if (r.top > r.bottom) std::swap(r.top, r.bottom);
    return r;
}
void label(HDC dc, const std::wstring& text, RECT rect, COLORREF color) {
    SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
Gdiplus::Color color(COLORREF value) { return Gdiplus::Color(255, GetRValue(value), GetGValue(value), GetBValue(value)); }
struct EdgeShape { GraphPoint a, b, c1, c2, labelAnchor; RECT weight, loop; bool selfLoop, curved, callout, aggregated; int source; };
double nodeRadius(const GraphViewState& view) {
    // Radius is in screen units; zoom changes spacing, never text/node size.
    return graphVertexRadius(view);
}
GraphPoint onCurve(const EdgeShape& edge, double t) {
    if (!edge.curved) return GraphPoint(edge.a.x + t * (edge.b.x - edge.a.x), edge.a.y + t * (edge.b.y - edge.a.y));
    double s = 1 - t;
    return GraphPoint(s*s*s*edge.a.x + 3*s*s*t*edge.c1.x + 3*s*t*t*edge.c2.x + t*t*t*edge.b.x,
                      s*s*s*edge.a.y + 3*s*s*t*edge.c1.y + 3*s*t*t*edge.c2.y + t*t*t*edge.b.y);
}
std::vector<EdgeShape> shapes(const Graph& graph, const GraphViewState& view, const RECT& bounds) {
    std::vector<EdgeShape> result;
    if (view.positions.empty()) return result;
    std::map<std::pair<int, int>, int> count, seen;
    double vertexRadius = nodeRadius(view);
    const auto clusters = graphClusters(view, bounds);
    std::vector<int> clusterOf(view.positions.size(), -1);
    for (std::size_t c = 0; c < clusters.size(); ++c)
        for (int v : clusters[c].vertices) clusterOf[v] = static_cast<int>(c);
    for (int source : view.visibleEdges) {
        const Edge& edge = graph.edges[source];
        ++count[{std::min(edge.from, edge.to), std::max(edge.from, edge.to)}];
    }
    for (int source : view.visibleEdges) {
        const Edge& edge = graph.edges[source];
        auto pair = std::make_pair(std::min(edge.from, edge.to), std::max(edge.from, edge.to));
        GraphPoint a = graphToScreen(view, bounds, view.positions[pair.first]);
        GraphPoint b = graphToScreen(view, bounds, view.positions[pair.second]);
        int firstCluster = clusterOf[pair.first], secondCluster = clusterOf[pair.second];
        bool firstGroup = firstCluster >= 0 && clusters[firstCluster].vertices.size() > 1;
        bool secondGroup = secondCluster >= 0 && clusters[secondCluster].vertices.size() > 1;
        if (firstGroup && firstCluster == secondCluster) continue;
        if (firstGroup) a = clusters[firstCluster].center;
        if (secondGroup) b = clusters[secondCluster].center;
        int index = seen[pair]++;
        EdgeShape shape = {}; shape.source = source; shape.selfLoop = pair.first == pair.second;
        shape.aggregated = firstGroup || secondGroup;
        double x, y;
        if (shape.selfLoop) {
            double offset = index * 13;
            double rx = 23, ry = 25;
            double cx = a.x + 12 + offset, cy = a.y - 33 - offset;
            shape.loop = {static_cast<LONG>(cx - rx), static_cast<LONG>(cy - ry), static_cast<LONG>(cx + rx), static_cast<LONG>(cy + ry)};
            x = cx; y = cy - ry;
        } else {
            double length = distance(a, b);
            // Bend the middle, never translate the endpoints away from vertices.
            double offset = (index - (count[pair] - 1) / 2.0) * 90 * view.zoom;
            GraphPoint control((a.x + b.x) / 2, (a.y + b.y) / 2);
            if (length > 0.01) { control.x -= (b.y - a.y) / length * offset; control.y += (b.x - a.x) / length * offset; }
            else { control.x += 70 * view.zoom; control.y -= 70 * view.zoom; }
            auto trim = [&](GraphPoint p, double boundary) {
                double d = distance(p, control);
                double radius = std::min(boundary, d * 0.45);
                if (d > 0.01) { p.x += (control.x - p.x) / d * radius; p.y += (control.y - p.y) / d * radius; }
                return p;
            };
            a = trim(a, firstGroup ? 32 : vertexRadius); b = trim(b, secondGroup ? 32 : vertexRadius);
            shape.a = a; shape.b = b; shape.curved = std::abs(offset) > 0.01 || length < 0.01;
            shape.c1 = GraphPoint(a.x + (control.x - a.x) * 2 / 3, a.y + (control.y - a.y) * 2 / 3);
            shape.c2 = GraphPoint(b.x + (control.x - b.x) * 2 / 3, b.y + (control.y - b.y) * 2 / 3);
            GraphPoint middle = onCurve(shape, 0.5); x = middle.x; y = middle.y;
        }
        int halfWidth = std::max(14, static_cast<int>(std::to_wstring(edge.weight).size()) * 5 + 8);
        shape.a = a; shape.b = b;
        shape.labelAnchor = GraphPoint(x, y);
        shape.weight = {static_cast<LONG>(x - halfWidth), static_cast<LONG>(y - 12), static_cast<LONG>(x + halfWidth), static_cast<LONG>(y + 12)};
        // Crossing edges can have the same midpoint. Move their labels along the
        // edge so every weight remains readable and has its own hit-test box.
        auto available = [&](RECT candidate) {
            if (candidate.left < bounds.left + 2 || candidate.right > bounds.right - 2 || candidate.top < bounds.top + 2 || candidate.bottom > bounds.bottom - 2) return false;
            for (GraphPoint position : view.positions) {
                GraphPoint center = graphToScreen(view, bounds, position);
                double nearestX = std::max(static_cast<double>(candidate.left), std::min(center.x, static_cast<double>(candidate.right)));
                double nearestY = std::max(static_cast<double>(candidate.top), std::min(center.y, static_cast<double>(candidate.bottom)));
                if (distance(center, GraphPoint(nearestX, nearestY)) < vertexRadius + 4) return false;
            }
            for (std::size_t previous = 0; previous < result.size(); ++previous) {
                if (!edgeLabelVisible(graph, view, result[previous].source)) continue;
                RECT intersection;
                if (IntersectRect(&intersection, &candidate, &result[previous].weight)) return false;
            }
            return true;
        };
        bool placed = shape.aggregated || (!edgeLabelVisible(graph, view, source) && graph.edges.size() > 100);
        for (int attempt = 0; !placed && attempt <= 12; ++attempt) {
            double delta = ((attempt + 1) / 2) * 0.06 * (attempt % 2 ? 1 : -1);
            GraphPoint candidatePoint = shape.selfLoop ? GraphPoint(x + delta * 240, y) : onCurve(shape, 0.5 + delta);
            RECT candidate = {static_cast<LONG>(candidatePoint.x - halfWidth), static_cast<LONG>(candidatePoint.y - 12),
                              static_cast<LONG>(candidatePoint.x + halfWidth), static_cast<LONG>(candidatePoint.y + 12)};
            if (available(candidate)) { shape.weight = candidate; placed = true; break; }
        }
        // Short crowded edges may have no space for a label on the path.
        // Put it nearby with a thin leader instead of covering a vertex.
        for (int ring = 1; !placed && ring <= 6; ++ring) for (int direction = 0; !placed && direction < 8; ++direction) {
            double cx = x + std::cos(direction * PI / 4) * ring * 24;
            double cy = y + std::sin(direction * PI / 4) * ring * 24;
            RECT candidate = {static_cast<LONG>(cx - halfWidth), static_cast<LONG>(cy - 12), static_cast<LONG>(cx + halfWidth), static_cast<LONG>(cy + 12)};
            if (available(candidate)) { shape.weight = candidate; shape.callout = true; placed = true; }
        }
        result.push_back(shape);
    }
    return result;
}
double segmentDistance(GraphPoint p, GraphPoint a, GraphPoint b) {
    double dx = b.x - a.x, dy = b.y - a.y, length2 = dx * dx + dy * dy;
    if (length2 < 0.01) return distance(p, a);
    double t = std::max(0.0, std::min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / length2));
    return distance(p, GraphPoint(a.x + t * dx, a.y + t * dy));
}
}

GraphPoint graphToScreen(const GraphViewState& view, const RECT& bounds, GraphPoint p) {
    return GraphPoint(bounds.left + p.x * view.zoom + view.pan.x, bounds.top + p.y * view.zoom + view.pan.y);
}
double graphVertexRadius(const GraphViewState& view) { return view.vertexRadius; }
std::vector<GraphCluster> graphClusters(const GraphViewState& view, const RECT& bounds) {
    // Semantic zoom: close projected nodes become a labelled group, not tiny dots.
    // This Union-Find is for screen grouping only; it never changes the graph.
    int n = static_cast<int>(view.positions.size()); UnionFind groups(n);
    std::vector<GraphPoint> points;
    for (GraphPoint p : view.positions) points.push_back(graphToScreen(view, bounds, p));
    double cell = view.vertexRadius * 2 + 8;
    std::map<std::pair<int, int>, std::vector<int>> buckets;
    for (int v = 0; v < n; ++v) {
        int x = static_cast<int>(std::floor(points[v].x / cell)), y = static_cast<int>(std::floor(points[v].y / cell));
        for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) {
            auto found = buckets.find({x + dx, y + dy});
            if (found != buckets.end()) for (int other : found->second)
                if (distance(points[v], points[other]) < cell) groups.unite(v, other);
        }
        buckets[{x, y}].push_back(v);
    }
    std::map<int, int> roots; std::vector<GraphCluster> result;
    for (int v = 0; v < n; ++v) {
        int root = groups.findRoot(v);
        auto found = roots.find(root);
        if (found == roots.end()) { roots[root] = static_cast<int>(result.size()); result.push_back(GraphCluster()); }
        GraphCluster& cluster = result[roots[root]];
        cluster.vertices.push_back(v); cluster.center.x += points[v].x; cluster.center.y += points[v].y;
    }
    for (GraphCluster& cluster : result) { cluster.center.x /= cluster.vertices.size(); cluster.center.y /= cluster.vertices.size(); }
    return result;
}
GraphPoint screenToGraph(const GraphViewState& view, const RECT& bounds, GraphPoint p) {
    return GraphPoint((p.x - bounds.left - view.pan.x) / view.zoom, (p.y - bounds.top - view.pan.y) / view.zoom);
}
void zoomGraph(GraphViewState& view, const RECT& bounds, double factor, GraphPoint anchor) {
    GraphPoint before = screenToGraph(view, bounds, anchor);
    view.zoom = std::max(0.02, std::min(4.0, view.zoom * factor));
    view.pan.x = anchor.x - bounds.left - before.x * view.zoom;
    view.pan.y = anchor.y - bounds.top - before.y * view.zoom;
}
void layoutGraph(GraphViewState& view, const Graph& graph, const RECT& bounds, GraphLayout layout) {
    view.positions.clear();
    view.selectedVertices.clear(); view.selectedEdges.clear();
    view.connectionStart = -1; view.selectingRectangle = false;
    view.hoveredVertex = -1; view.hoveredEdge = -1; view.hoveredCluster = -1;
    view.zoom = 1; view.pan = GraphPoint();
    double width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    int n = std::min(graph.vertexCount, GuiLimits::DRAW_VERTICES), columns = static_cast<int>(std::ceil(std::sqrt(n)));
    // Use a scrollable world with sufficient spacing, rather than shrinking nodes.
    if (layout == GraphLayout::Grid && n > 0) {
        width = std::max(width, (columns + 1) * 80.0);
        height = std::max(height, ((n + columns - 1) / columns + 1) * 80.0);
    }
    if (layout == GraphLayout::Circle) {
        double diameter = n > 8 ? std::max(0.0, n * 64.0 / PI) : 0;
        width = std::max(width, diameter + 130); height = std::max(height, diameter + 130);
    }
    double radius = std::max(20.0, std::min(width, height) / 2 - 65);
    view.pan = GraphPoint((bounds.right - bounds.left - width) / 2, (bounds.bottom - bounds.top - height) / 2);
    for (int v = 0; v < n; ++v) {
        GraphPoint p(width / 2, height / 2);
        if (n > 1 && layout == GraphLayout::Circle) {
            double angle = -PI / 2 + 2 * PI * v / n;
            p = GraphPoint(static_cast<LONG>(width / 2 + radius * std::cos(angle)), static_cast<LONG>(height / 2 + radius * std::sin(angle)));
        } else if (n > 1) {
            int rows = (n + columns - 1) / columns;
            double cellWidth = width / (columns + 1), cellHeight = height / (rows + 1);
            p = GraphPoint((v % columns + 1) * cellWidth, (v / columns + 1) * cellHeight);
        }
        view.positions.push_back(p);
    }
    updateVisibleEdges(view, graph);
}
void updateVisibleEdges(GraphViewState& view, const Graph& graph) {
    view.visibleEdges.clear(); view.hasMstCache = false; view.mstSourceEdges.clear();
    for (std::size_t i = 0; i < graph.edges.size() && view.visibleEdges.size() < GuiLimits::DRAW_EDGES; ++i) {
        const Edge& edge = graph.edges[i];
        if (edge.from >= 0 && edge.to >= 0 && edge.from < static_cast<int>(view.positions.size()) && edge.to < static_cast<int>(view.positions.size()))
            view.visibleEdges.push_back(static_cast<int>(i));
    }
}
void cacheMstEdges(GraphViewState& view, const Graph& graph, const MSTResult& result) {
    using Key = std::tuple<int, int, int>;
    auto key = [](const Edge& e) { return Key(std::min(e.from, e.to), std::max(e.from, e.to), e.weight); };
    std::map<Key, std::vector<int>> visible;
    for (int index : view.visibleEdges) visible[key(graph.edges[index])].push_back(index);
    view.mstSourceEdges.clear();
    for (const Edge& e : result.edges) {
        auto found = visible.find(key(e));
        if (found != visible.end() && !found->second.empty()) {
            view.mstSourceEdges.insert(found->second.front()); found->second.erase(found->second.begin());
        }
    }
    view.hasMstCache = true;
}
namespace {
bool focusedEdge(const Graph& graph, const GraphViewState& view, int index) {
    const Edge& edge = graph.edges[index];
    if (!view.selectedVertices.empty() || !view.selectedEdges.empty())
        return view.selectedEdges.count(index) || view.selectedVertices.count(edge.from) || view.selectedVertices.count(edge.to);
    if (view.hoveredVertex >= 0) return edge.from == view.hoveredVertex || edge.to == view.hoveredVertex;
    return view.hoveredEdge == index;
}
}
bool edgeLabelVisible(const Graph& graph, const GraphViewState& view, int index) {
    if (index < 0 || index >= static_cast<int>(graph.edges.size())) return false;
    // The checkbox applies to every edge, independent of zoom and edge count.
    // When unchecked, selection/hover can still reveal an individual weight.
    return view.showAllWeights || focusedEdge(graph, view, index);
}
GraphHit hitTestGraph(const Graph& graph, const GraphViewState& view, const RECT& bounds, POINT cursor) {
    GraphHit hit;
    if (!PtInRect(&bounds, cursor) || graph.vertexCount > GuiLimits::MAX_VERTICES || graph.edges.size() > GuiLimits::MAX_EDGES) return hit;
    double vertexRadius = nodeRadius(view);
    const auto clusters = graphClusters(view, bounds);
    double nearestVertex = 1e30;
    for (std::size_t i = 0; i < clusters.size(); ++i) {
        double d = distance(GraphPoint(cursor.x, cursor.y), clusters[i].center);
        if (d <= (clusters[i].vertices.size() > 1 ? 32 : vertexRadius) && d < nearestVertex) {
            nearestVertex = d;
            hit.vertex = clusters[i].vertices.size() == 1 ? clusters[i].vertices[0] : -1;
            hit.cluster = clusters[i].vertices.size() > 1 ? static_cast<int>(i) : -1;
        }
    }
    if (hit.vertex >= 0 || hit.cluster >= 0) return hit;
    std::vector<EdgeShape> edges = shapes(graph, view, bounds);
    for (std::size_t i = 0; i < edges.size(); ++i)
        if (!edges[i].aggregated && edgeLabelVisible(graph, view, edges[i].source) && PtInRect(&edges[i].weight, cursor)) { hit.edge = edges[i].source; hit.edgeLabel = true; return hit; }
    double nearest = 7;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const EdgeShape& edge = edges[i];
        if (edge.aggregated) continue;
        double d;
        if (edge.selfLoop) {
            double rx = (edge.loop.right - edge.loop.left) / 2.0, ry = (edge.loop.bottom - edge.loop.top) / 2.0;
            double cx = (edge.loop.left + edge.loop.right) / 2.0, cy = (edge.loop.top + edge.loop.bottom) / 2.0;
            d = std::abs(std::hypot((cursor.x - cx) / rx, (cursor.y - cy) / ry) - 1) * std::min(rx, ry);
        } else {
            d = 1e30;
            GraphPoint before = edge.a;
            for (int sample = 1; sample <= 32; ++sample) {
                GraphPoint after = onCurve(edge, sample / 32.0);
                d = std::min(d, segmentDistance(GraphPoint(cursor.x, cursor.y), before, after)); before = after;
            }
        }
        if (d < nearest) { nearest = d; hit.edge = edges[i].source; }
    }
    return hit;
}
RECT graphEdgeLabel(const Graph& graph, const GraphViewState& view, const RECT& bounds, int index) {
    std::vector<EdgeShape> edges = shapes(graph, view, bounds);
    for (const EdgeShape& edge : edges) if (edge.source == index && !edge.aggregated) return edge.weight;
    return RECT{};
}
std::vector<GraphPoint> graphEdgePath(const Graph& graph, const GraphViewState& view, const RECT& bounds, int index) {
    auto edges = shapes(graph, view, bounds); std::vector<GraphPoint> path;
    for (const EdgeShape& edge : edges) if (edge.source == index && !edge.selfLoop)
        for (int sample = 0; sample <= 32; ++sample) path.push_back(onCurve(edge, sample / 32.0));
    return path;
}
void selectGraphRectangle(const Graph& graph, GraphViewState& view, const RECT& bounds, RECT rectangle) {
    rectangle = normalized(rectangle);
    for (std::size_t i = 0; i < view.positions.size(); ++i) {
        POINT p = point(graphToScreen(view, bounds, view.positions[i]));
        if (PtInRect(&rectangle, p)) view.selectedVertices.insert(static_cast<int>(i));
    }
    const auto edges = shapes(graph, view, bounds);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        POINT center = {(edges[i].weight.left + edges[i].weight.right) / 2, (edges[i].weight.top + edges[i].weight.bottom) / 2};
        if (PtInRect(&rectangle, center)) view.selectedEdges.insert(edges[i].source);
    }
}

void fitGraph(GraphViewState& view, const Graph& graph, const RECT& bounds) {
    if (view.positions.empty() || bounds.right - bounds.left < 100 || bounds.bottom - bounds.top < 100) return;
    // Fit actual curves and loops, not just vertex centers. Their screen-size
    // decorations stay fixed, so refine after each zoom/pan adjustment.
    for (int iteration = 0; iteration < 8; ++iteration) {
        double left = 1e30, top = 1e30, right = -1e30, bottom = -1e30;
        auto include = [&](GraphPoint p, double margin) {
            left = std::min(left, p.x - margin); right = std::max(right, p.x + margin);
            top = std::min(top, p.y - margin); bottom = std::max(bottom, p.y + margin);
        };
        for (GraphPoint p : view.positions) include(graphToScreen(view, bounds, p), 36);
        for (const auto& edge : shapes(graph, view, bounds)) {
            if (edge.selfLoop) { include(GraphPoint(edge.loop.left, edge.loop.top), 8); include(GraphPoint(edge.loop.right, edge.loop.bottom), 8); }
            else { include(edge.a, 8); include(edge.b, 8); include(edge.c1, 8); include(edge.c2, 8); }
            if (!edge.aggregated && edgeLabelVisible(graph, view, edge.source)) {
                include(GraphPoint(edge.weight.left, edge.weight.top), 8); include(GraphPoint(edge.weight.right, edge.weight.bottom), 8);
            }
        }
        double factor = std::min((bounds.right - bounds.left - 24) / (right - left), (bounds.bottom - bounds.top - 24) / (bottom - top));
        double newZoom = std::max(0.02, std::min(4.0, view.zoom * factor));
        GraphPoint world = screenToGraph(view, bounds, GraphPoint((left + right) / 2, (top + bottom) / 2));
        view.zoom = newZoom;
        view.pan = GraphPoint((bounds.right - bounds.left) / 2.0 - world.x * view.zoom,
                             (bounds.bottom - bounds.top) / 2.0 - world.y * view.zoom);
    }
}

void drawGraphInteractive(HDC dc, const RECT& bounds, const Graph& graph, const MSTResult* result, HFONT font, const GraphViewState& view) {
    int saved = SaveDC(dc);
    IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    SelectObject(dc, font);
    if (graph.vertexCount > GuiLimits::MAX_VERTICES || graph.edges.size() > GuiLimits::MAX_EDGES) {
        label(dc, L"Dữ liệu vượt giới hạn bộ nhớ hỗ trợ.", bounds, muted);
        RestoreDC(dc, saved); return;
    }
    if (graph.vertexCount == 0) {
        label(dc, L"Chuột phải để thêm đỉnh. Kéo chuột phải giữa hai đỉnh để nối cạnh.", bounds, muted);
    }
    if (view.positions.size() != static_cast<std::size_t>(std::min(graph.vertexCount, GuiLimits::DRAW_VERTICES))) { RestoreDC(dc, saved); return; }
    const auto edges = shapes(graph, view, bounds);
    using Key = std::tuple<int, int, int>;
    auto key = [](const Edge& edge) { return Key(std::min(edge.from, edge.to), std::max(edge.from, edge.to), edge.weight); };
    std::map<Key, int> chosen;
    if (result && !view.hasMstCache) for (const Edge& edge : result->edges) ++chosen[key(edge)];
    std::vector<bool> mst(edges.size(), false);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (result && view.hasMstCache) mst[i] = view.mstSourceEdges.count(edges[i].source) != 0;
        else if (chosen[key(graph.edges[edges[i].source])] > 0) { mst[i] = true; --chosen[key(graph.edges[edges[i].source])]; }
    }
    for (int pass = 0; pass < 4; ++pass) for (std::size_t i = 0; i < edges.size(); ++i) {
        bool selected = view.selectedEdges.count(edges[i].source) != 0;
        bool relevant = focusedEdge(graph, view, edges[i].source);
        if ((selected ? 3 : relevant ? 2 : mst[i] ? 1 : 0) != pass) continue;
        const EdgeShape& edge = edges[i];
        double minX = std::min(std::min(edge.a.x, edge.b.x), std::min(edge.c1.x, edge.c2.x));
        double maxX = std::max(std::max(edge.a.x, edge.b.x), std::max(edge.c1.x, edge.c2.x));
        double minY = std::min(std::min(edge.a.y, edge.b.y), std::min(edge.c1.y, edge.c2.y));
        double maxY = std::max(std::max(edge.a.y, edge.b.y), std::max(edge.c1.y, edge.c2.y));
        if (!edge.selfLoop && (maxX < bounds.left || minX > bounds.right || maxY < bounds.top || minY > bounds.bottom)) continue;
        Gdiplus::Graphics graphics(dc); graphics.SetPageUnit(Gdiplus::UnitPixel);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        COLORREF edgeColor = selected ? red : mst[i] ? blue : relevant ? ink : RGB(170, 185, 205);
        Gdiplus::Pen pen(color(edgeColor), selected || mst[i] ? 3.0f : relevant ? 2.2f : 1.0f);
        if (edge.selfLoop) graphics.DrawEllipse(&pen, static_cast<int>(edge.loop.left), static_cast<int>(edge.loop.top), static_cast<int>(edge.loop.right - edge.loop.left), static_cast<int>(edge.loop.bottom - edge.loop.top));
        else if (edge.curved) graphics.DrawBezier(&pen, static_cast<float>(edge.a.x), static_cast<float>(edge.a.y),
            static_cast<float>(edge.c1.x), static_cast<float>(edge.c1.y), static_cast<float>(edge.c2.x), static_cast<float>(edge.c2.y),
            static_cast<float>(edge.b.x), static_cast<float>(edge.b.y));
        else graphics.DrawLine(&pen, static_cast<float>(edge.a.x), static_cast<float>(edge.a.y), static_cast<float>(edge.b.x), static_cast<float>(edge.b.y));
    }
    for (std::size_t i = 0; i < edges.size(); ++i) {
        int source = edges[i].source;
        if (edges[i].aggregated) continue;
        if (!edgeLabelVisible(graph, view, source)) continue;
        RECT visible; if (!IntersectRect(&visible, &bounds, &edges[i].weight)) continue;
        if (edges[i].callout) {
            HPEN leader = CreatePen(PS_SOLID, 1, RGB(148, 163, 184)); HGDIOBJ previous = SelectObject(dc, leader);
            MoveToEx(dc, static_cast<int>(edges[i].labelAnchor.x), static_cast<int>(edges[i].labelAnchor.y), nullptr);
            LineTo(dc, (edges[i].weight.left + edges[i].weight.right) / 2, (edges[i].weight.top + edges[i].weight.bottom) / 2);
            SelectObject(dc, previous); DeleteObject(leader);
        }
        HPEN outline = CreatePen(PS_SOLID, 1, RGB(215, 223, 235));
        HGDIOBJ oldPen = SelectObject(dc, outline), oldBrush = SelectObject(dc, GetStockObject(WHITE_BRUSH));
        RoundRect(dc, edges[i].weight.left, edges[i].weight.top, edges[i].weight.right, edges[i].weight.bottom, 7, 7);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(outline);
        label(dc, std::to_wstring(graph.edges[source].weight), edges[i].weight, view.selectedEdges.count(source) ? red : mst[i] ? blue : muted);
    }
    const auto clusters = graphClusters(view, bounds);
    for (std::size_t index = 0; index < clusters.size(); ++index) {
        const GraphCluster& cluster = clusters[index];
        bool group = cluster.vertices.size() > 1;
        int v = cluster.vertices.front();
        GraphPoint p = cluster.center;
        double radius = group ? 32 : nodeRadius(view);
        if (p.x + radius < bounds.left || p.x - radius > bounds.right || p.y + radius < bounds.top || p.y - radius > bounds.bottom) continue;
        bool selected = false;
        for (int member : cluster.vertices) if (view.selectedVertices.count(member)) selected = true;
        bool hovered = group ? view.hoveredCluster == static_cast<int>(index) : view.hoveredVertex == v;
        {
            Gdiplus::Graphics graphics(dc); graphics.SetPageUnit(Gdiplus::UnitPixel);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::Pen pen(color(selected ? red : hovered || group || result ? blue : RGB(148, 163, 184)), selected ? 3.0f : 2.0f);
            Gdiplus::SolidBrush brush(color(selected ? RGB(254, 242, 242) : result ? RGB(239, 246, 255) : RGB(248, 250, 252)));
            graphics.FillEllipse(&brush, static_cast<float>(p.x - radius), static_cast<float>(p.y - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2));
            graphics.DrawEllipse(&pen, static_cast<float>(p.x - radius), static_cast<float>(p.y - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2));
        }
        RECT box = {static_cast<LONG>(p.x - radius), static_cast<LONG>(p.y - radius), static_cast<LONG>(p.x + radius), static_cast<LONG>(p.y + radius)};
        if (group) {
            RECT count = box; count.bottom = static_cast<LONG>(p.y + 3);
            RECT description = box; description.top = static_cast<LONG>(p.y + 1);
            label(dc, std::to_wstring(cluster.vertices.size()), count, blue);
            label(dc, L"đỉnh", description, muted);
        } else {
            std::wstring name = vertexLabel(view.labels, v);
            SIZE size; GetTextExtentPoint32W(dc, name.c_str(), static_cast<int>(name.size()), &size);
            HFONT smaller = nullptr; HGDIOBJ previous = nullptr;
            if (size.cx > radius * 2 - 8) {
                LOGFONTW description = {}; GetObjectW(GetCurrentObject(dc, OBJ_FONT), sizeof(description), &description);
                description.lfHeight = -std::max(10, static_cast<int>(std::abs(description.lfHeight) * (radius * 2 - 8) / size.cx));
                smaller = CreateFontIndirectW(&description); previous = SelectObject(dc, smaller);
            }
            label(dc, name, box, ink);
            if (smaller) { SelectObject(dc, previous); DeleteObject(smaller); }
        }
    }
    if (view.connectionStart >= 0 && view.connectionStart < static_cast<int>(view.positions.size())) {
        POINT a = point(graphToScreen(view, bounds, view.positions[view.connectionStart]));
        HPEN pen = CreatePen(PS_DASH, 1, red); HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, a.x, a.y, nullptr); LineTo(dc, view.cursor.x, view.cursor.y);
        SelectObject(dc, old); DeleteObject(pen);
    }
    if (view.selectingRectangle) {
        RECT rect = normalized(view.selectionRectangle);
        HPEN pen = CreatePen(PS_DOT, 1, blue); HGDIOBJ old = SelectObject(dc, pen);
        HGDIOBJ brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(dc, old); SelectObject(dc, brush); DeleteObject(pen);
    }
    RestoreDC(dc, saved);
}

// Existing public function stays compatible with all rendering tests.
void drawGraph(HDC dc, const RECT& bounds, const Graph& graph, const MSTResult* result, HFONT font) {
    GraphViewState view;
    view.vertexRadius = 20;
    layoutGraph(view, graph, bounds, GraphLayout::Circle);
    if (result) cacheMstEdges(view, graph, *result);
    drawGraphInteractive(dc, bounds, graph, result, font, view);
}

namespace GraphEditing {
void addVertex(Graph& graph, GraphViewState& view, const RECT& bounds, const GraphPoint* at) {
    if (graph.vertexCount >= GuiLimits::DRAW_VERTICES) throw std::invalid_argument("Dùng bộ sinh hoặc mở file để thêm dữ liệu lớn; canvas chỉ chỉnh trực tiếp 1.000 đỉnh đầu.");
    GraphPoint position;
    if (at) position = *at;
    else {
        double width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        // Search visible grid slots, checking the current screen positions for overlap.
        double best = -1;
        for (int row = 1; row <= 8; ++row) for (int col = 1; col <= 8; ++col) {
            GraphPoint screen(bounds.left + col * width / 9, bounds.top + row * height / 9);
            GraphPoint candidate = screenToGraph(view, bounds, screen);
            double nearest = 1e9;
            for (GraphPoint existing : view.positions) nearest = std::min(nearest, distance(candidate, existing) * view.zoom);
            if (nearest > best) { best = nearest; position = candidate; }
        }
    }
    ++graph.vertexCount; view.positions.push_back(position);
    view.selectedVertices.clear(); view.selectedEdges.clear();
    view.selectedVertices.insert(graph.vertexCount - 1);
}
void addEdge(Graph& graph, int from, int to, int weight) {
    if (from < 0 || to < 0 || from >= graph.vertexCount || to >= graph.vertexCount)
        throw std::invalid_argument("Hai đầu cạnh phải nằm trong khoảng 0 đến V-1.");
    if (graph.vertexCount > GuiLimits::MAX_VERTICES || graph.edges.size() >= GuiLimits::MAX_EDGES)
        throw std::invalid_argument("Dữ liệu vượt giới hạn 1.000.000 đỉnh hoặc 2.000.000 cạnh.");
    int parallel = 0;
    for (const Edge& edge : graph.edges)
        if (std::min(edge.from, edge.to) == std::min(from, to) && std::max(edge.from, edge.to) == std::max(from, to)) ++parallel;
    if (parallel >= GuiLimits::MAX_PARALLEL) throw std::invalid_argument("Mỗi cặp đỉnh chỉ được có tối đa 3 cạnh song song.");
    graph.edges.push_back({std::min(from, to), std::max(from, to), weight});
}
}
namespace GuiLimits {
void validate(const Graph& graph) {
    GraphInput::validate(graph);
    if (graph.vertexCount > MAX_VERTICES || graph.edges.size() > MAX_EDGES)
        throw std::invalid_argument("Dữ liệu vượt giới hạn 1.000.000 đỉnh hoặc 2.000.000 cạnh.");
    std::map<std::pair<int, int>, int> counts;
    for (const Edge& edge : graph.edges)
        if (++counts[{std::min(edge.from, edge.to), std::max(edge.from, edge.to)}] > MAX_PARALLEL)
            throw std::invalid_argument("Mỗi cặp đỉnh chỉ được có tối đa 3 cạnh song song.");
}
}
namespace GraphEditing {
bool deleteSelected(Graph& graph, GraphViewState& view) {
    if (view.selectedVertices.empty() && view.selectedEdges.empty()) return false;
    std::vector<int> remap(graph.vertexCount, -1);
    std::vector<GraphPoint> positions;
    int count = 0;
    for (int v = 0; v < graph.vertexCount; ++v) if (!view.selectedVertices.count(v)) {
        remap[v] = count++;
        if (v < static_cast<int>(view.positions.size())) positions.push_back(view.positions[v]);
    }
    std::vector<Edge> edges;
    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const Edge& edge = graph.edges[i];
        if (!view.selectedEdges.count(static_cast<int>(i)) && remap[edge.from] >= 0 && remap[edge.to] >= 0)
            edges.push_back({remap[edge.from], remap[edge.to], edge.weight});
    }
    VertexLabels labels;
    for (const auto& entry : view.labels) if (remap[entry.first] >= 0) labels[remap[entry.first]] = entry.second;
    view.labels.swap(labels);
    graph.vertexCount = count; graph.edges.swap(edges); view.positions.swap(positions);
    view.selectedVertices.clear(); view.selectedEdges.clear(); view.connectionStart = -1;
    return true;
}
}
