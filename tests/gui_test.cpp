// Exercise our own hidden window without driving the user's desktop.
#include "../src/gui/main.cpp"
#include <iostream>

namespace {
int checks = 0;
void check(bool ok, const char* message) {
    ++checks; if (!ok) throw std::runtime_error(message);
}
void waitWorker(HWND window) {
    auto start = std::chrono::steady_clock::now();
    while (workerBusy) {
        MSG message;
        while (PeekMessageW(&message, window, WORK_DONE, WORK_DONE, PM_REMOVE)) DispatchMessageW(&message);
        if (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() > 30) throw std::runtime_error("Worker timeout");
        Sleep(1);
    }
}
void checkFits(HWND window) {
    fitView(window); RECT canvas = drawingBounds(window);
    for (GraphPoint p : view.positions) {
        p = graphToScreen(view, canvas, p);
        check(p.x - 24 >= canvas.left && p.x + 24 <= canvas.right && p.y - 24 >= canvas.top && p.y + 24 <= canvas.bottom, "Vertex clipped after fit");
    }
    for (int edge : view.visibleEdges) for (GraphPoint p : graphEdgePath(currentGraph, view, canvas, edge))
        check(p.x >= canvas.left && p.x <= canvas.right && p.y >= canvas.top && p.y <= canvas.bottom, "Parallel curve clipped after fit");
}
}
int main() {
    Gdiplus::GdiplusStartupInput startup; ULONG_PTR token; Gdiplus::GdiplusStartup(&token, &startup, nullptr);
    WNDCLASSW wc = {}; wc.lpfnWndProc = windowProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"KruskalGuiTests";
    wc.style = CS_DBLCLKS; RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Tests", WS_OVERLAPPEDWINDOW, 0, 0, 1280, 980, nullptr, nullptr, wc.hInstance, nullptr);
    try {
        check(window != nullptr, "Window creation failed");
        int combos = 0;
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            wchar_t type[32]; GetClassNameW(child, type, 32);
            if (std::wstring(type) == L"ComboBox") ++combos;
            check(textOf(child) != L"Nhập nhanh", "Removed quick-input button still exists");
        }
        check(combos == 1, "Layout selector still exists");
        RECT resultBounds; GetWindowRect(resultEdit, &resultBounds); POINT resultPoint = {resultBounds.left, resultBounds.top}; ScreenToClient(window, &resultPoint);
        check(resultPoint.x >= drawingBounds(window).left && resultPoint.y > drawingBounds(window).bottom, "Result is not below canvas");
        check(SendMessageW(samples, CB_GETCOUNT, 0, 0) == sampleCount + 1, "Invalid-input demo still in sample list");
        Graph graph; graph.vertexCount = 6; graph.edges = {{0, 1, 2}, {2, 3, 1}, {2, 3, 1}, {2, 3, 1}, {4, 4, -5}};
        putGraph(window, graph, L"Fit regression"); checkFits(window);
        check(std::abs(view.positions[0].y - view.positions[1].y) < 0.01, "Default layout is not grid");
        SetWindowPos(window, nullptr, 0, 0, 1120, 900, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); checkFits(window);
        dpi = 144; updateControlFonts(window);
        SetWindowPos(window, nullptr, 0, 0, scaled(1120), scaled(900), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); checkFits(window);
        dpi = 96; updateControlFonts(window);
        SetWindowPos(window, nullptr, 0, 0, 1280, 980, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        check(!resultReady, "Fit or load ran Kruskal");
        double oldPanX = view.pan.x, oldPanY = view.pan.y;
        SendMessageW(window, WM_HSCROLL, SB_LINERIGHT, reinterpret_cast<LPARAM>(canvasHScroll));
        check(view.pan.x < oldPanX && !resultReady, "Horizontal scrollbar did not pan canvas");
        SendMessageW(window, WM_VSCROLL, SB_LINEDOWN, reinterpret_cast<LPARAM>(canvasVScroll));
        check(view.pan.y < oldPanY && !resultReady, "Vertical scrollbar did not pan canvas");
        checkFits(window);
        beginVertexEdit(window, 0); SetWindowTextW(weightEditor, L"A"); finishWeightEdit(window, true);
        check(vertexLabel(view.labels, 0) == L"A", "Rename not applied");
        runKruskal(window); long long weight = currentResult.totalWeight;
        beginVertexEdit(window, 1); SetWindowTextW(weightEditor, L"Hà Nội"); finishWeightEdit(window, true);
        check(resultReady && currentResult.totalWeight == weight && textOf(resultEdit).find(L"Hà Nội") != std::wstring::npos, "Rename changed MST or failed to refresh output");
        beginVertexEdit(window, 1); SetWindowTextW(weightEditor, L"A"); finishWeightEdit(window, true);
        check(vertexLabel(view.labels, 1) == L"Hà Nội", "Duplicate name accepted");
        check(vertexIndex(view.labels, 6, L"A") == 0 && vertexIndex(view.labels, 6, L"1") == 1, "Name/index resolution failed");
        bool rejected = false;
        try { renameVertex(view.labels, 6, 2, L"1"); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Name can shadow numeric vertex ID");
        VertexLabels decoded; std::ostringstream numeric; writeGraph(numeric, graph);
        Graph restored = readGuiFile(writeVertexLabels(view.labels) + numeric.str(), &decoded);
        check(decoded == view.labels && restored.edges.size() == graph.edges.size(), "Named file round-trip failed");
        beginCellEdit(window, 0, 1); SetWindowTextW(tableCellEditor, L"A"); finishCellEdit(window, true);
        check(currentGraph.edges[0].to == 0 && !resultReady, "Table name endpoint edit failed");
        putGraph(window, graph, L"Delete regression", true, decoded);
        view.selectedVertices.insert(0); deleteSelection(window);
        check(vertexLabel(view.labels, 0) == L"Hà Nội" && !resultReady, "Deleting vertex lost surviving names");
        SetWindowTextW(generatorVertices, L"64"); SetWindowTextW(generatorEdges, L"161"); generate(window);
        check(currentGraph.vertexCount == 64 && currentGraph.edges.size() == 161 && view.labels.empty() && !resultReady, "Generator did not replace data cleanly");
        runKruskal(window); check(resultReady && currentResult.edges.size() == 63, "Generated network result invalid");
        SetWindowTextW(generatorVertices, L"20000"); SetWindowTextW(generatorEdges, L"100000"); generate(window); waitWorker(window);
        runKruskal(window); waitWorker(window);
        check(currentGraph.edges.size() == 100000 && currentResult.edges.size() == 19999 && resultReady, "Large-data worker result truncated");
        if (sampleCount == 7) {
        SendMessageW(samples, CB_SETCURSEL, 6, 0); loadSample(window); runKruskal(window);
        check(currentGraph.vertexCount == 64 && currentGraph.edges.size() == 161 && currentResult.totalWeight == 786 && currentResult.edges.size() == 63, "Realistic network fixture differs from documented result");
        }
        clearGraph(window); check(currentGraph.vertexCount == 0 && view.labels.empty() && !resultReady, "Clear failed");
        addVertexAt(window); check(currentGraph.vertexCount == 1 && !resultReady, "Blank canvas cannot add isolated vertex");
        runKruskal(window); check(currentResult.hasSpanningTree && currentResult.totalWeight == 0, "Singleton failed");
        std::cout << "PASS: " << checks << " GUI integration assertions.\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; DestroyWindow(window); return 1; }
    DestroyWindow(window);
    for (HFONT font : {bodyFont, headingFont, controlFont, controlHeadingFont}) DeleteObject(font);
    DeleteObject(whiteBrush); DeleteObject(backgroundBrush);
    Gdiplus::GdiplusShutdown(token); return 0;
}
