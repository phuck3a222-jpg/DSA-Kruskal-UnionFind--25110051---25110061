#define UNICODE
#define _UNICODE
#include <windows.h>
#include "GraphInput.h"
#include "Kruskal.h"
#include <sstream>
#include <string>
#include <vector>
#include <exception>

namespace {
enum { RUN = 100, SAMPLE, CLEAR };
HWND vertices, edgeCount, edges, output;

std::wstring getText(HWND control) {
    int length = GetWindowTextLengthW(control);
    std::vector<wchar_t> buffer(length + 1);
    GetWindowTextW(control, buffer.data(), length + 1);
    return std::wstring(buffer.data());
}

HWND control(HWND parent, const wchar_t* type, const wchar_t* text,
             DWORD style, int x, int y, int width, int height, int id = 0) {
    HWND handle = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(handle, WM_SETFONT,
        reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return handle;
}

void loadSample() {
    SetWindowTextW(vertices, L"4");
    SetWindowTextW(edgeCount, L"5");
    SetWindowTextW(edges, L"0 1 1\r\n1 2 2\r\n0 2 3\r\n2 3 4\r\n1 3 5");
    SetWindowTextW(output, L"Sample loaded. Expected total weight: 7; components: 1.");
}

void run(HWND window) {
    SetWindowTextW(output, L"");
    try {
        std::wstring source = getText(vertices) + L" " + getText(edgeCount) + L"\n" + getText(edges);
        // Graph input accepts ASCII integer tokens only; do not silently narrow Unicode.
        std::string ascii;
        for (wchar_t ch : source) {
            if (ch > 127) {
                throw std::invalid_argument("Use integer digits, spaces and line breaks only.");
            }
            ascii.push_back(static_cast<char>(ch));
        }
        std::istringstream input(ascii);
        Graph graph = GraphInput::read(input);
        MSTResult result = Kruskal().solve(graph);
        std::wostringstream text;
        if (graph.vertexCount == 0) {
            text << L"Empty graph: no spanning tree.\r\n";
        } else if (result.hasSpanningTree) {
            text << L"Connected graph - Minimum spanning tree (MST)\r\n";
        } else {
            text << L"Disconnected graph - Minimum spanning forest\r\n";
        }
        text << L"Total weight: " << result.totalWeight
             << L"\r\nComponents: " << result.componentCount
             << L"\r\nSelected edges: " << result.edges.size()
             << L"\r\n\r\nFrom\tTo\tWeight\r\n";
        // Keep the native text control responsive for very large results.
        std::size_t shown = 0;
        for (const Edge& edge : result.edges) {
            if (shown == 10000) {
                text << L"... Showing first 10000 selected edges. Use console for full output.\r\n";
                break;
            }
            text << edge.from << L"\t" << edge.to << L"\t" << edge.weight << L"\r\n";
            ++shown;
        }
        SetWindowTextW(output, text.str().c_str());
    } catch (const std::exception& error) {
        std::string message = error.what();
        std::wstring wide(message.begin(), message.end());
        SetWindowTextW(output, wide.c_str());
        MessageBoxW(window, wide.c_str(), L"Cannot run Kruskal", MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        control(window, L"STATIC", L"Vertices (V):", 0, 16, 18, 95, 22);
        vertices = control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 112, 16, 100, 26);
        control(window, L"STATIC", L"Edges (E):", 0, 232, 18, 80, 22);
        edgeCount = control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 312, 16, 100, 26);
        SendMessageW(vertices, EM_SETLIMITTEXT, 16, 0);
        SendMessageW(edgeCount, EM_SETLIMITTEXT, 16, 0);
        control(window, L"BUTTON", L"Run Kruskal", WS_TABSTOP, 440, 14, 130, 30, RUN);
        control(window, L"BUTTON", L"Load sample", WS_TABSTOP, 580, 14, 110, 30, SAMPLE);
        control(window, L"BUTTON", L"Clear", WS_TABSTOP, 700, 14, 75, 30, CLEAR);
        control(window, L"STATIC", L"Edges: from to weight (one per line). Vertex indices: 0..V-1.", 0, 16, 60, 755, 24);
        edges = control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN, 16, 90, 350, 400);
        output = control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY, 386, 90, 389, 400);
        SendMessageW(edges, EM_SETLIMITTEXT, 16 * 1024 * 1024, 0);
        SendMessageW(output, EM_SETLIMITTEXT, 2 * 1024 * 1024, 0);
        control(window, L"STATIC", L"Negative weights, parallel edges and self-loops are allowed. Empty graph: V=0, E=0.", 0, 16, 506, 760, 24);
        loadSample();
        return 0;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            switch (LOWORD(wParam)) {
            case RUN: run(window); break;
            case SAMPLE: loadSample(); break;
            case CLEAR:
                SetWindowTextW(vertices, L"");
                SetWindowTextW(edgeCount, L"");
                SetWindowTextW(edges, L"");
                SetWindowTextW(output, L"");
                SetFocus(vertices);
                break;
            }
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    const wchar_t* className = L"DSAWeek2Kruskal";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowW(className, L"Kruskal MST - Week 2", WS_OVERLAPPED | WS_CAPTION |
        WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 810, 580,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message;
    int status;
    while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return status == -1 ? 1 : static_cast<int>(message.wParam);
}
