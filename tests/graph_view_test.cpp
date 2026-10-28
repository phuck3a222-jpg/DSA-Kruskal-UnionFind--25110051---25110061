#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include "gui/GraphView.h"
#include <iostream>
#include <stdexcept>

bool allWeightsRendered() {
    Graph graph; graph.vertexCount = 64;
    for (int v = 0; v < 64; ++v) {
        if (v % 8 < 7) graph.edges.push_back({v, v + 1, 10});
        if (v < 56) graph.edges.push_back({v, v + 8, 12});
        if (v < 56 && v % 8 < 7) graph.edges.push_back({v, v + 9, 18});
    }
    RECT bounds = {0, 0, 1600, 1600}; GraphViewState view;
    layoutGraph(view, graph, bounds, GraphLayout::Grid);
    HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 1600, 1600); ReleaseDC(nullptr, screen);
    HGDIOBJ old = SelectObject(dc, bitmap);
    HFONT font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    drawGraphInteractive(dc, bounds, graph, nullptr, font, view);
    int rendered = 0;
    for (int i = 0; i < static_cast<int>(graph.edges.size()); ++i) {
        RECT label = graphEdgeLabel(graph, view, bounds, i); bool ink = false;
        for (int y = label.top + 4; y < label.bottom - 4 && !ink; ++y)
            for (int x = label.left + 4; x < label.right - 4 && !ink; ++x) {
                COLORREF pixel = GetPixel(dc, x, y);
                ink = GetRValue(pixel) < 130 && GetGValue(pixel) < 150 && GetBValue(pixel) < 180;
            }
        if (ink) ++rendered;
    }
    view.zoom = 0.4;
    bool explicitOn = true;
    for (int i = 0; i < 161; ++i) explicitOn = explicitOn && edgeLabelVisible(graph, view, i);
    view.showAllWeights = false;
    bool explicitOff = true;
    for (int i = 0; i < 161; ++i) explicitOff = explicitOff && !edgeLabelVisible(graph, view, i);
    SelectObject(dc, old); DeleteObject(bitmap); DeleteObject(font); DeleteDC(dc);
    std::cout << "Rendered weights: " << rendered << "/161\n";
    return rendered == 161 && explicitOn && explicitOff;
}

bool selectionKeepsGraphVisible() {
    Graph graph; graph.vertexCount = 4;
    graph.edges = {{0, 1, 1}, {1, 2, 2}, {0, 2, 3}, {2, 3, 4}, {1, 3, 5}};
    RECT bounds = {0, 0, 800, 600};
    GraphViewState view;
    layoutGraph(view, graph, bounds, GraphLayout::Circle);
    std::vector<RECT> labels;
    for (int i = 0; i < 5; ++i) labels.push_back(graphEdgeLabel(graph, view, bounds, i));
    for (int selected = 0; selected < 5; ++selected) {
        view.selectedEdges.clear(); view.selectedEdges.insert(selected);
        for (int i = 0; i < 5; ++i) {
            RECT actual = graphEdgeLabel(graph, view, bounds, i);
            if (!edgeLabelVisible(graph, view, i) || !EqualRect(&labels[i], &actual) ||
                graphEdgePath(graph, view, bounds, i).empty()) return false;
        }
    }
    view.selectedEdges.clear(); view.hoveredVertex = 0;
    for (int i = 0; i < 5; ++i) if (!edgeLabelVisible(graph, view, i)) return false;
    graph.vertexCount = 10; view.showAllWeights = true;
    for (int i = 0; i < 5; ++i) if (!edgeLabelVisible(graph, view, i)) return false;
    view.zoom = 0.1;
    if (graphVertexRadius(view) != 24) return false;
    view.zoom = 4;
    return graphVertexRadius(view) == 24;
}

// Draw into an off-screen bitmap at four DPI values. This catches double scaling:
// the circle and its number must occupy the same expected device coordinates.
int main() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
    bool passed = selectionKeepsGraphVisible() && allWeightsRendered();
    std::cout << (passed ? "PASS" : "FAIL") << ": selection preserves other edges and labels\n";
    for (int dpi : {96, 120, 144, 192}) {
        auto scale = [=](int value) { return MulDiv(value, dpi, 96); };
        HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
        HBITMAP image = CreateCompatibleBitmap(screen, scale(800), scale(600));
        ReleaseDC(nullptr, screen);
        HGDIOBJ oldImage = SelectObject(dc, image);
        RECT pixels = {0, 0, scale(800), scale(600)};
        FillRect(dc, &pixels, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, 96, 96, nullptr); SetViewportExtEx(dc, dpi, dpi, nullptr);
        HFONT font = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
        Graph graph; graph.vertexCount = 1;
        MSTResult result; result.componentCount = 1; result.hasSpanningTree = true;
        RECT bounds = {100, 100, 700, 500};
        drawGraph(dc, bounds, graph, &result, font);
        SetMapMode(dc, MM_TEXT);
        int bluePixels = 0, numberPixels = 0;
        for (int x = scale(417); x <= scale(423); ++x)
            for (int y = scale(296); y <= scale(304); ++y) {
                COLORREF pixel = GetPixel(dc, x, y);
                if (GetBValue(pixel) > 180 && GetRValue(pixel) < 100) ++bluePixels;
            }
        for (int x = scale(394); x <= scale(406); ++x)
            for (int y = scale(291); y <= scale(309); ++y) {
                COLORREF pixel = GetPixel(dc, x, y);
                if (GetRValue(pixel) < 100 && GetGValue(pixel) < 130 && GetBValue(pixel) < 160) ++numberPixels;
            }
        bool ok = bluePixels > 0 && numberPixels > 0;
        passed = passed && ok;
        std::cout << (ok ? "PASS" : "FAIL") << ": aligned circle and label at DPI " << dpi << '\n';
        SelectObject(dc, oldImage); DeleteObject(font); DeleteObject(image); DeleteDC(dc);
    }
    Gdiplus::GdiplusShutdown(token);
    return passed ? 0 : 1;
}
