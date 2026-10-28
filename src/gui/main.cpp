#define UNICODE
#define _UNICODE
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <gdiplus.h>
#include "GraphInput.h"
#include "GraphGenerator.h"
#include "Kruskal.h"
#include "GraphView.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <climits>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <memory>
#include <vector>

namespace {
enum { RUN = 100, OPEN, SAVE, GENERATE, SAMPLES, CLEAR,
    ADD_VERTEX, EDIT_EDGE, DELETE_SELECTED, RENAME_VERTEX,
    ZOOM_OUT, ZOOM_IN, ZOOM_RESET, WEIGHT_PLUS, WEIGHT_MINUS, SHOW_WEIGHTS, FIT_VIEW, FOCUS_CLUSTER };
const COLORREF background = RGB(242, 245, 250), ink = RGB(25, 42, 65), muted = RGB(71, 85, 105);
const COLORREF accent = RGB(37, 99, 235), border = RGB(222, 229, 238);
HFONT bodyFont, headingFont;
HFONT controlFont, controlHeadingFont;
int dpi = 96;
int scaled(int value) { return MulDiv(value, dpi, 96); }
HBRUSH whiteBrush, backgroundBrush;
HWND resultEdit, samples, compression;
HWND minEdit, maxEdit, connected;
HWND generatorVertices, generatorEdges, fitButton;
std::wstring generatorHint;
HWND edgeTable, tableCellEditor;
WNDPROC originalCellProc;
int tableTopRow = 0, cellRow = -1, cellColumn = -1;
std::mt19937 seedSource(static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
const UINT STATUS_TIMER = 701;
HWND openButton, saveButton, clearButton, runButton, generateButton;
HWND showWeights;
bool trackingCanvas = false;
HWND zoomOutButton, zoomInButton, zoomResetButton, weightEditor;
HWND canvasHScroll, canvasVScroll;
WNDPROC originalWeightProc;
Graph currentGraph;
MSTResult currentResult;
bool graphReady = false, resultReady = false;
bool resultCompression = true;
bool workerBusy = false, closeWhenDone = false;
std::thread worker;
const UINT WORK_DONE = WM_APP + 901;
struct WorkResult {
    Graph graph;
    VertexLabels labels;
    MSTResult result;
    int kind = 0; // 0: Kruskal, 1: generate, 2: open, 3: save
    double milliseconds = 0;
    std::string error;
    std::wstring name;
};
GraphViewState view;
enum class MouseAction { None, MoveVertices, Rectangle, Pan, Connect };
MouseAction mouseAction = MouseAction::None;
POINT mouseStart = {}, previousMouse = {};
int editingEdge = -1, editingVertex = -1;
const wchar_t* editedStatus = L"Đã sửa đồ thị. Bấm Chạy Kruskal để tính lại.";
std::wstring status = L"Chọn mẫu hoặc nhập dữ liệu để bắt đầu.";
double solveMs = 0;
void updateHints(HWND window);
void fitView(HWND window);
void refreshResult();
void updateCanvasScrollbars(HWND window);
long long field(HWND edit);
void refreshTable();
void finishCellEdit(HWND window, bool save);
LRESULT CALLBACK tableProc(HWND, UINT, WPARAM, LPARAM);
void startWork(HWND window, int kind, Graph graph, GeneratorOptions options, bool useCompression, const std::wstring& path);

RECT drawingBounds(HWND window) {
    RECT r; GetClientRect(window, &r);
    RECT drawing = {406, 72, MulDiv(r.right, 96, dpi) - 46, MulDiv(r.bottom, 96, dpi) - 320};
    return drawing;
}
POINT mousePoint(LPARAM value) {
    POINT p = {MulDiv(GET_X_LPARAM(value), 96, dpi), MulDiv(GET_Y_LPARAM(value), 96, dpi)};
    return p;
}
void cancelInteraction(HWND window) {
    mouseAction = MouseAction::None; view.connectionStart = -1;
    view.selectingRectangle = false;
    if (GetCapture() == window) ReleaseCapture();
}
void resetView(HWND window) {
    cancelInteraction(window);
    layoutGraph(view, currentGraph, drawingBounds(window), GraphLayout::Grid);
    if (resultReady) cacheMstEdges(view, currentGraph, currentResult);
    fitView(window);
}
void syncControls() {
    updateVisibleEdges(view, currentGraph);
    refreshTable();
}
void updateControlFonts(HWND window) {
    HFONT oldBody = controlFont, oldHeading = controlHeadingFont;
    controlFont = CreateFontW(-scaled(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    controlHeadingFont = CreateFontW(-scaled(15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), TRUE);
    if (oldBody) DeleteObject(oldBody);
    if (oldHeading) DeleteObject(oldHeading);
}

const wchar_t* sampleNames[] = {
    L"01  Đồ thị cơ bản", L"02  Không liên thông", L"03  Trọng số âm & cạnh song song",
    L"04  Các cạnh cùng trọng số", L"05  Có đỉnh cô lập", L"06  Một đỉnh", L"07  Mạng mô phỏng 64 nút"
};
const wchar_t* sampleFiles[] = {
    L"small_graph.txt", L"disconnected_graph.txt", L"week2/negative_parallel_loops.txt",
    L"week2/equal_weights.txt", L"week2/isolated_vertex.txt", L"week2/single_vertex.txt", L"network_64.txt"
};
int sampleCount = 6;
std::wstring samplePath(int index) {
    wchar_t module[32768]; GetModuleFileNameW(nullptr, module, 32768);
    std::wstring path(module);
    return path.substr(0, path.find_last_of(L"\\/") + 1) + L"examples/" + sampleFiles[index];
}


std::wstring textOf(HWND handle) {
    std::vector<wchar_t> buffer(GetWindowTextLengthW(handle) + 1);
    GetWindowTextW(handle, buffer.data(), static_cast<int>(buffer.size()));
    return buffer.data();
}
std::wstring wide(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size);
    return result;
}
std::wstring localizedError(const std::exception& error) {
    std::string text = error.what();
    if (text.find("Invalid vertex index for edge ") == 0)
        return L"Chỉ số đỉnh của cạnh " + wide(text.substr(30, text.find('.', 30) - 30)) + L" không hợp lệ. Đỉnh phải nằm trong khoảng 0 đến V-1.";
    if (text.find("Invalid or missing data for edge") == 0) return L"Dữ liệu cạnh còn thiếu hoặc sai định dạng. Mỗi dòng cần đúng ba số nguyên: đỉnh đầu, đỉnh cuối, trọng số.";
    if (text.find("Unexpected data after") == 0) return L"File có dữ liệu thừa sau số cạnh đã khai báo. Kiểm tra lại header V E và danh sách cạnh.";
    if (text == "stoll" || text.find("integers") != std::string::npos) return L"Vui lòng nhập số nguyên hợp lệ vào các ô số.";
    if (text.find("supported limits") != std::string::npos || text.find("between 0 and") != std::string::npos) return L"Số đỉnh hoặc số cạnh vượt giới hạn cho phép.";
    if (text.find("Invalid generator") == 0) return L"Thông số sinh đồ thị không hợp lệ. Kiểm tra số đỉnh, số cạnh và khoảng trọng số.";
    if (text.find("Endpoints must") == 0) return L"Hai đầu cạnh phải nằm trong khoảng 0 đến V-1.";
    if (text.find("Could not write graph") == 0) return L"Không thể ghi đủ dữ liệu ra file. Kiểm tra quyền ghi và dung lượng ổ đĩa.";
    if (text.find("Cannot open graph file") == 0) return L"Không thể mở file đồ thị. Kiểm tra đường dẫn và quyền đọc.";
    if (text.find("Could not read") == 0) return L"Không đọc được toàn bộ file. Hãy chọn lại file đồ thị hợp lệ.";
    if (text.find("Cannot create") == 0 || text.find("Could not save") == 0) return L"Không lưu được file. Kiểm tra đường dẫn, quyền ghi và dung lượng ổ đĩa.";
    if (text.find_first_of("\x80\x81\xC3\xC4\xC5\xC6\xE1\xE2") == std::string::npos)
        return L"Dữ liệu không hợp lệ. Kiểm tra số đỉnh, số cạnh và các dòng cạnh (ba số nguyên mỗi dòng).";
    return wide(text);
}
void updateHints(HWND window) {
    bool canGenerate = false;
    try {
        long long v = field(generatorVertices), e = field(generatorEdges);
        if (v < 1 || v > GuiLimits::MAX_VERTICES) throw std::invalid_argument("Nhập từ 1 đến 1.000.000 đỉnh để sinh đồ thị.");
        bool connectedGraph = SendMessageW(connected, BM_GETCHECK, 0, 0) == BST_CHECKED;
        long long minimum = connectedGraph ? v - 1 : 0;
        long long maximum = std::min(static_cast<long long>(GuiLimits::MAX_EDGES), v * (v - 1) / 2);
        generatorHint = L"Số cạnh hợp lệ: " + std::to_wstring(minimum) + L" – " + std::to_wstring(maximum) + L".";
        long long low = field(minEdit), high = field(maxEdit);
        canGenerate = v > 0 && e >= minimum && e <= maximum && low >= INT_MIN && high <= INT_MAX && low <= high;
        if (low > high) generatorHint = L"Trọng số nhỏ nhất không được lớn hơn trọng số lớn nhất.";
        else if (low < INT_MIN || high > INT_MAX) generatorHint = L"Trọng số phải nằm trong phạm vi số nguyên 32-bit.";
    } catch (const std::exception& error) { generatorHint = localizedError(error); }
    if (generateButton) EnableWindow(generateButton, canGenerate && !workerBusy);
    InvalidateRect(window, nullptr, FALSE);
}
std::string ascii(const std::wstring& text) {
    std::string output;
    for (wchar_t ch : text) {
        if (ch > 127) throw std::invalid_argument("Dữ liệu cạnh chỉ nhận số nguyên, dấu cách và xuống dòng.");
        output.push_back(static_cast<char>(ch));
    }
    return output;
}
long long field(HWND edit) {
    std::string text = ascii(textOf(edit));
    std::size_t used = 0;
    long long value = std::stoll(text, &used);
    if (used != text.size()) throw std::invalid_argument("Các ô số phải chứa đúng một số nguyên.");
    return value;
}
void invalidateResult(HWND window) {
    resultReady = false;
    SetWindowTextW(resultEdit, L"");
    status = editedStatus;
    SetTimer(window, STATUS_TIMER, 5000, nullptr);
    InvalidateRect(window, nullptr, FALSE);
}
void graphChanged(HWND window) {
    graphReady = true;
    view.hoveredVertex = -1; view.hoveredEdge = -1; view.hoveredCluster = -1;
    syncControls(); invalidateResult(window);
    updateHints(window);
}
std::string readFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Không thể mở file đồ thị. Kiểm tra đường dẫn và quyền đọc.");
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 128 * 1024 * 1024) {
        CloseHandle(file); throw std::runtime_error("File vượt quá giới hạn 128 MiB của giao diện.");
    }
    std::string text(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    bool ok = ReadFile(file, text.empty() ? nullptr : &text[0], static_cast<DWORD>(text.size()), &read, nullptr) != 0;
    CloseHandle(file);
    if (!ok || read != text.size()) throw std::runtime_error("Không đọc được toàn bộ file đồ thị.");
    // Accept UTF-8 BOM, otherwise graph data is plain ASCII integer tokens.
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.erase(0, 3);
    return text;
}
Graph readGuiFile(const std::string& source, VertexLabels* labels = nullptr) {
    std::istringstream lines(source); std::string line; std::ostringstream numeric;
    while (std::getline(lines, line)) if (line.compare(0, 7, "# name ") != 0) numeric << line << '\n';
    std::istringstream input(numeric.str());
    Graph graph = GraphInput::read(input); GuiLimits::validate(graph);
    std::string data = source; VertexLabels parsed = readVertexLabels(data, graph.vertexCount);
    if (labels) *labels = std::move(parsed);
    return graph;
}
void writeGuiFile(const std::wstring& path, const Graph& graph, const VertexLabels& labels) {
    std::ostringstream text; text << writeVertexLabels(labels); writeGraph(text, graph); std::string data = text.str();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Không thể tạo file. Kiểm tra đường dẫn và quyền ghi.");
    DWORD written = 0;
    bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) != 0;
    CloseHandle(file);
    if (!ok || written != data.size()) throw std::runtime_error("Không ghi được toàn bộ file đồ thị.");
}
void showError(HWND window, const std::exception& error) {
    resultReady = false;
    status = L"Không thể xử lý dữ liệu. Kiểm tra thông báo bên dưới.";
    std::wstring message = localizedError(error);
    KillTimer(window, STATUS_TIMER);
    SetWindowTextW(resultEdit, message.c_str());
    InvalidateRect(window, nullptr, FALSE);
    MessageBoxW(window, message.c_str(), L"Dữ liệu chưa hợp lệ", MB_OK | MB_ICONWARNING);
}
void putGraph(HWND window, const Graph& graph, const std::wstring& name, bool validated = false, const VertexLabels& labels = VertexLabels()) {
    if (!validated) GuiLimits::validate(graph);
    currentGraph = graph;
    view.labels = labels;
    graphReady = true; resultReady = false;
    syncControls(); resetView(window);
    SetWindowTextW(resultEdit, L"Dữ liệu đã nạp. Nhấn Chạy Kruskal để tìm cây khung nhỏ nhất.");
    status = name + L" · Nhấp đúp đỉnh để đổi tên.";
    updateHints(window); SetTimer(window, STATUS_TIMER, 5000, nullptr);
    InvalidateRect(window, nullptr, FALSE);
}
void loadSample(HWND window) {
    int selected = static_cast<int>(SendMessageW(samples, CB_GETCURSEL, 0, 0));
    if (selected == sampleCount) {
        Graph empty; putGraph(window, empty, L"Bảng trắng"); return;
    }
    if (selected < 0 || selected >= sampleCount) return;
    std::string source = readFile(samplePath(selected));
    putGraph(window, readGuiFile(source), sampleNames[selected]);
}
bool chooseFile(HWND window, bool save, std::wstring& path) {
    wchar_t buffer[32768] = L"";
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Đồ thị (*.txt)\0*.txt\0Tất cả file (*.*)\0*.*\0";
    dialog.lpstrTitle = save ? L"Lưu đồ thị" : L"Mở đồ thị";
    dialog.lpstrFile = buffer; dialog.nMaxFile = 32768; dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
        (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) return false;
    path = buffer; return true;
}
Graph readEditor() {
    return currentGraph;
}
void refreshResult() {
    if (!resultReady) return;
    std::wostringstream report;
    report << (currentGraph.vertexCount == 0 ? L"Đồ thị rỗng: không có MST."
        : currentResult.hasSpanningTree ? L"CÂY KHUNG NHỎ NHẤT (MST)" : L"RỪNG KHUNG · không liên thông")
        << L"\r\nTổng trọng số: " << currentResult.totalWeight
        << L"\r\nCạnh chọn: " << currentResult.edges.size() << L" · Thành phần: " << currentResult.componentCount
        << L"\r\nPath compression: " << (resultCompression ? L"BẬT" : L"TẮT")
        << L" · " << std::fixed << std::setprecision(3) << solveMs << L" ms\r\n\r\n";
    for (std::size_t i = 0; i < currentResult.edges.size() && i < 10000; ++i) {
        const Edge& edge = currentResult.edges[i];
        report << vertexLabel(view.labels, edge.from) << L" — " << vertexLabel(view.labels, edge.to) << L" : " << edge.weight << L"\r\n";
    }
    if (currentResult.edges.size() > 10000) report << L"Hiện 10.000 cạnh đầu.\r\n";
    SetWindowTextW(resultEdit, report.str().c_str());
}
void presentResult(HWND window, const Graph& graph, const MSTResult& result, double milliseconds) {
    solveMs = milliseconds; resultCompression = SendMessageW(compression, BM_GETCHECK, 0, 0) == BST_CHECKED;
    currentGraph = graph; currentResult = result; graphReady = true; resultReady = true;
    if (view.positions.size() != static_cast<std::size_t>(std::min(graph.vertexCount, GuiLimits::DRAW_VERTICES))) resetView(window);
    cacheMstEdges(view, graph, result); refreshResult();
    status = L"Đã tính xong."; SetTimer(window, STATUS_TIMER, 5000, nullptr);
    InvalidateRect(window, nullptr, FALSE);
}
void runKruskal(HWND window) {
    Graph graph = readEditor();
    bool useCompression = SendMessageW(compression, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (graph.vertexCount > 5000 || graph.edges.size() > 20000) {
        startWork(window, 0, std::move(graph), GeneratorOptions(), useCompression, L""); return;
    }
    auto start = std::chrono::steady_clock::now();
    MSTResult result = Kruskal().solve(graph, useCompression);
    double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    presentResult(window, graph, result, milliseconds);
}
void generate(HWND window) {
    GeneratorOptions options;
    long long v = field(generatorVertices), e = field(generatorEdges), lo = field(minEdit), hi = field(maxEdit);
    if (v < 1 || v > GuiLimits::MAX_VERTICES) throw std::invalid_argument("Để sinh đồ thị, số đỉnh phải từ 1 đến 1.000.000.");
    bool mustConnect = SendMessageW(connected, BM_GETCHECK, 0, 0) == BST_CHECKED;
    long long maxEdges = std::min(static_cast<long long>(GuiLimits::MAX_EDGES), v * (v - 1) / 2), minEdges = mustConnect ? v - 1 : 0;
    if (e < minEdges || e > maxEdges)
        throw std::invalid_argument("Số cạnh để sinh đồ thị với " + std::to_string(v) + " đỉnh phải từ " + std::to_string(minEdges) + " đến " + std::to_string(maxEdges) + ".");
    if (lo < INT_MIN || hi > INT_MAX || lo > hi) throw std::invalid_argument("Khoảng trọng số không hợp lệ: số nhỏ nhất không được lớn hơn số lớn nhất, và cả hai phải là số nguyên 32-bit.");
    options.vertices = static_cast<int>(v); options.edges = static_cast<int>(e);
    options.minWeight = static_cast<int>(lo); options.maxWeight = static_cast<int>(hi);
    options.seed = seedSource(); options.connected = mustConnect;
    if (v > 5000 || e > 20000) startWork(window, 1, Graph(), options, false, L"");
    else putGraph(window, generateGraph(options), L"Đồ thị ngẫu nhiên");
}
void setBusy(HWND window, bool busy) {
    workerBusy = busy;
    for (HWND control : {samples, openButton, saveButton, clearButton, runButton, generateButton,
                         generatorVertices, generatorEdges, minEdit, maxEdit, connected, compression, edgeTable})
        if (control) EnableWindow(control, !busy);
    if (!busy) updateHints(window);
    else KillTimer(window, STATUS_TIMER);
    InvalidateRect(window, nullptr, FALSE);
}
void startWork(HWND window, int kind, Graph graph, GeneratorOptions options, bool useCompression, const std::wstring& path) {
    if (workerBusy) return;
    if (worker.joinable()) worker.join();
    WorkResult* task = new WorkResult; task->graph = std::move(graph); task->kind = kind; task->name = path; task->labels = view.labels;
    setBusy(window, true);
    status = kind == 0 ? L"Đang tính Kruskal trên toàn bộ dữ liệu..." : kind == 1 ? L"Đang sinh đồ thị..." : kind == 2 ? L"Đang đọc và kiểm tra file..." : L"Đang lưu toàn bộ dữ liệu...";
    try {
        worker = std::thread([window, task, options, useCompression]() {
            try {
                auto start = std::chrono::steady_clock::now();
                if (task->kind == 0) task->result = Kruskal().solve(task->graph, useCompression);
                else if (task->kind == 1) { task->graph = generateGraph(options); GuiLimits::validate(task->graph); }
                else if (task->kind == 2) task->graph = readGuiFile(readFile(task->name), &task->labels);
                else writeGuiFile(task->name, task->graph, task->labels);
                task->milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            } catch (const std::exception& error) { task->error = error.what(); }
            if (!PostMessageW(window, WORK_DONE, 0, reinterpret_cast<LPARAM>(task))) delete task;
        });
    } catch (...) { delete task; setBusy(window, false); throw; }
}
void saveGraph(HWND window) {
    Graph graph = readEditor();
    std::wstring path;
    if (!chooseFile(window, true, path)) return;
    if (graph.edges.size() > 20000) { startWork(window, 3, std::move(graph), GeneratorOptions(), false, path); return; }
    writeGuiFile(path, graph, view.labels);
    status = L"Đã lưu đồ thị: " + path; InvalidateRect(window, nullptr, FALSE);
}

void requireEditable() {
    if (workerBusy) throw std::invalid_argument("Đang xử lý dữ liệu. Bạn có thể zoom hoặc di chuyển khung nhìn; chờ xử lý xong để sửa đồ thị.");


}
void finishWeightEdit(HWND window, bool save) {
    if (editingVertex >= 0) {
        int vertex = editingVertex; editingVertex = -1;
        if (save) {
            try { renameVertex(view.labels, currentGraph.vertexCount, vertex, textOf(weightEditor)); refreshTable(); refreshResult(); }
            catch (const std::exception& error) { status = localizedError(error); }
        }
        ShowWindow(weightEditor, SW_HIDE); InvalidateRect(window, nullptr, FALSE); return;
    }
    if (editingEdge < 0) return;
    int index = editingEdge; editingEdge = -1;
    if (save && index < static_cast<int>(currentGraph.edges.size())) {
        try {
            long long value = field(weightEditor);
            if (value < INT_MIN || value > INT_MAX) throw std::invalid_argument("Trọng số phải là số nguyên có dấu 32-bit.");
            if (currentGraph.edges[index].weight != value) {
                currentGraph.edges[index].weight = static_cast<int>(value);
                graphChanged(window);
            }
        } catch (const std::exception&) {
            status = L"Trọng số không hợp lệ; giữ nguyên giá trị cũ (số nguyên 32-bit).";
            InvalidateRect(window, nullptr, FALSE);
        }
    }
    ShowWindow(weightEditor, SW_HIDE);
}
LRESULT CALLBACK weightProc(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
    HWND window = GetParent(edit);
    if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if (message == WM_KEYDOWN && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        finishWeightEdit(window, wParam == VK_RETURN); SetFocus(window); return 0;
    }
    if (message == WM_KILLFOCUS) finishWeightEdit(window, true);
    return CallWindowProcW(originalWeightProc, edit, message, wParam, lParam);
}
void beginWeightEdit(HWND window, int index) {
    requireEditable(); finishWeightEdit(window, true);
    if (index < 0 || index >= static_cast<int>(currentGraph.edges.size())) return;
    RECT box = graphEdgeLabel(currentGraph, view, drawingBounds(window), index);
    RECT canvas = drawingBounds(window);
    int x = std::max(static_cast<int>(canvas.left), std::min(static_cast<int>(box.left), static_cast<int>(canvas.right) - 110));
    int y = std::max(static_cast<int>(canvas.top), std::min(static_cast<int>(box.top) - 3, static_cast<int>(canvas.bottom) - 30));
    MoveWindow(weightEditor, scaled(x), scaled(y), scaled(110), scaled(30), TRUE);
    SetWindowTextW(weightEditor, std::to_wstring(currentGraph.edges[index].weight).c_str());
    editingEdge = index;
    ShowWindow(weightEditor, SW_SHOW); SetFocus(weightEditor); SendMessageW(weightEditor, EM_SETSEL, 0, -1);
    status = L"Nhập trọng số: Enter/click ra ngoài để lưu; Esc để hủy.";
    InvalidateRect(window, nullptr, FALSE);
}
void beginVertexEdit(HWND window, int vertex) {
    requireEditable(); finishWeightEdit(window, true);
    if (vertex < 0 || vertex >= static_cast<int>(view.positions.size())) return;
    RECT canvas = drawingBounds(window); GraphPoint p = graphToScreen(view, canvas, view.positions[vertex]);
    int x = std::max(static_cast<int>(canvas.left), std::min(static_cast<int>(p.x) - 70, static_cast<int>(canvas.right) - 150));
    int y = std::max(static_cast<int>(canvas.top), std::min(static_cast<int>(p.y) + 28, static_cast<int>(canvas.bottom) - 30));
    MoveWindow(weightEditor, scaled(x), scaled(y), scaled(150), scaled(30), TRUE);
    editingVertex = vertex; SetWindowTextW(weightEditor, vertexLabel(view.labels, vertex).c_str());
    ShowWindow(weightEditor, SW_SHOW); SetFocus(weightEditor); SendMessageW(weightEditor, EM_SETSEL, 0, -1);
}
void addVertexAt(HWND window, const GraphPoint* at = nullptr) {
    requireEditable(); finishWeightEdit(window, true);
    GraphEditing::addVertex(currentGraph, view, drawingBounds(window), at);
    graphChanged(window);
}
void connectVertices(HWND window, int from, int to) {
    requireEditable();
    GraphEditing::addEdge(currentGraph, from, to, 1);
    view.selectedVertices.clear(); view.selectedEdges.clear();
    view.selectedEdges.insert(static_cast<int>(currentGraph.edges.size()) - 1);
    graphChanged(window);
}
void deleteSelection(HWND window) {
    requireEditable(); finishWeightEdit(window, true); cancelInteraction(window);
    if (GraphEditing::deleteSelected(currentGraph, view)) {
        if (view.positions.size() != static_cast<std::size_t>(std::min(currentGraph.vertexCount, GuiLimits::DRAW_VERTICES))) resetView(window);
        graphChanged(window);
    }
}
void clearGraph(HWND window) {
    finishWeightEdit(window, false);
    Graph empty; putGraph(window, empty, L"Bảng trắng");
    SendMessageW(samples, CB_SETCURSEL, sampleCount, 0);
    status = editedStatus; InvalidateRect(window, nullptr, FALSE);
}
void adjustZoom(HWND window, double factor) {
    finishWeightEdit(window, true);
    RECT canvas = drawingBounds(window);
    zoomGraph(view, canvas, factor, GraphPoint((canvas.left + canvas.right) / 2.0, (canvas.top + canvas.bottom) / 2.0));
    SetWindowTextW(zoomResetButton, (std::to_wstring(static_cast<int>(std::lround(view.zoom * 100))) + L"%").c_str());
    InvalidateRect(window, nullptr, FALSE);
}
void fitView(HWND window) {
    fitGraph(view, currentGraph, drawingBounds(window));
    if (zoomResetButton) SetWindowTextW(zoomResetButton, (std::to_wstring(static_cast<int>(std::lround(view.zoom * 100))) + L"%").c_str());
    updateCanvasScrollbars(window);
    InvalidateRect(window, nullptr, FALSE);
}
void focusCluster(HWND window, int index) {
    RECT canvas = drawingBounds(window);
    const auto clusters = graphClusters(view, canvas);
    if (index < 0 || index >= static_cast<int>(clusters.size())) return;
    const auto& members = clusters[index].vertices;
    GraphPoint center;
    double minimum = 1e30;
    for (int v : members) {
        center.x += view.positions[v].x; center.y += view.positions[v].y;
        for (int other : members) if (other < v)
            minimum = std::min(minimum, std::hypot(view.positions[v].x - view.positions[other].x, view.positions[v].y - view.positions[other].y));
    }
    center.x /= members.size(); center.y /= members.size();
    double spacing = 2 * view.vertexRadius + 16;
    view.zoom = std::min(4.0, std::max(view.zoom * 1.8, spacing / std::max(0.01, minimum)));
    // Imported or manually arranged positions may coincide. Spread only this
    // visual group if zoom alone cannot separate it; Graph/MST stay untouched.
    if (minimum * view.zoom < spacing) {
        int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(members.size()))));
        int rows = (static_cast<int>(members.size()) + columns - 1) / columns;
        for (std::size_t i = 0; i < members.size(); ++i)
            view.positions[members[i]] = GraphPoint(center.x + (static_cast<int>(i) % columns - (columns - 1) / 2.0) * spacing / view.zoom,
                center.y + (static_cast<int>(i) / columns - (rows - 1) / 2.0) * spacing / view.zoom);
    }
    view.pan = GraphPoint((canvas.right - canvas.left) / 2.0 - center.x * view.zoom,
                         (canvas.bottom - canvas.top) / 2.0 - center.y * view.zoom);
    view.hoveredCluster = -1; view.hoveredVertex = -1; view.hoveredEdge = -1;
    SetWindowTextW(zoomResetButton, (std::to_wstring(static_cast<int>(std::lround(view.zoom * 100))) + L"%").c_str());
    InvalidateRect(window, nullptr, FALSE);
}
void contextMenu(HWND window, POINT cursor) {
    requireEditable();
    RECT canvas = drawingBounds(window);
    if (!PtInRect(&canvas, cursor)) return;
    GraphHit hit = hitTestGraph(currentGraph, view, canvas, cursor);
    HMENU menu = CreatePopupMenu();
    if (hit.cluster >= 0) {
        AppendMenuW(menu, MF_STRING, FOCUS_CLUSTER, L"Phóng to nhóm đỉnh");
    } else if (hit.vertex >= 0) {
        view.selectedVertices.clear(); view.selectedEdges.clear(); view.selectedVertices.insert(hit.vertex);
        AppendMenuW(menu, MF_STRING, RENAME_VERTEX, L"Đổi tên đỉnh...");
        std::wstring title = L"Xóa đỉnh " + vertexLabel(view.labels, hit.vertex);
        AppendMenuW(menu, MF_STRING, DELETE_SELECTED, title.c_str());
    } else if (hit.edge >= 0) {
        view.selectedVertices.clear(); view.selectedEdges.clear(); view.selectedEdges.insert(hit.edge);
        AppendMenuW(menu, MF_STRING, WEIGHT_PLUS, L"Tăng trọng số (+1)");
        AppendMenuW(menu, MF_STRING, WEIGHT_MINUS, L"Giảm trọng số (-1)");
        AppendMenuW(menu, MF_STRING, EDIT_EDGE, L"Nhập trọng số mới...");
        AppendMenuW(menu, MF_STRING, DELETE_SELECTED, L"Xóa cạnh");
    } else AppendMenuW(menu, MF_STRING, ADD_VERTEX, L"+ Thêm đỉnh tại đây");
    InvalidateRect(window, nullptr, FALSE);
    POINT screen = {scaled(cursor.x), scaled(cursor.y)}; ClientToScreen(window, &screen);
    int choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (choice == FOCUS_CLUSTER) focusCluster(window, hit.cluster);
    else if (choice == ADD_VERTEX) { GraphPoint at = screenToGraph(view, canvas, GraphPoint(cursor.x, cursor.y)); addVertexAt(window, &at); }
    else if (choice == RENAME_VERTEX) beginVertexEdit(window, hit.vertex);
    else if (choice == DELETE_SELECTED) deleteSelection(window);
    else if (choice == EDIT_EDGE) beginWeightEdit(window, hit.edge);
    else if (choice == WEIGHT_PLUS || choice == WEIGHT_MINUS) {
        long long weight = static_cast<long long>(currentGraph.edges[hit.edge].weight) + (choice == WEIGHT_PLUS ? 1 : -1);
        if (weight < INT_MIN || weight > INT_MAX) throw std::invalid_argument("Không thể tăng hoặc giảm: trọng số đã đạt giới hạn số nguyên 32-bit.");
        currentGraph.edges[hit.edge].weight = static_cast<int>(weight); graphChanged(window);
    }
}

void leftDown(HWND window, POINT cursor, bool ctrl) {
    RECT canvas = drawingBounds(window);
    if (!PtInRect(&canvas, cursor)) return;
    finishWeightEdit(window, true); requireEditable(); SetFocus(window);
    GraphHit hit = hitTestGraph(currentGraph, view, canvas, cursor);
    if (hit.cluster >= 0) { focusCluster(window, hit.cluster); return; }
    mouseStart = previousMouse = cursor;
    if (hit.vertex >= 0) {
        bool selected = view.selectedVertices.count(hit.vertex) != 0;
        if (!ctrl && !selected) { view.selectedVertices.clear(); view.selectedEdges.clear(); }
        if (ctrl && selected) view.selectedVertices.erase(hit.vertex);
        else { view.selectedVertices.insert(hit.vertex); mouseAction = MouseAction::MoveVertices; SetCapture(window); }
    } else if (hit.edge >= 0) {
        bool selected = view.selectedEdges.count(hit.edge) != 0;
        if (!ctrl) { view.selectedVertices.clear(); view.selectedEdges.clear(); }
        if (!(ctrl && selected)) view.selectedEdges.insert(hit.edge); else view.selectedEdges.erase(hit.edge);
    } else {
        if (!ctrl) { view.selectedVertices.clear(); view.selectedEdges.clear(); }
        view.selectionRectangle = {cursor.x, cursor.y, cursor.x, cursor.y};
        view.selectingRectangle = true; mouseAction = MouseAction::Rectangle; SetCapture(window);
    }
    InvalidateRect(window, nullptr, FALSE);
}
void mouseMove(HWND window, POINT cursor) {
    RECT canvas = drawingBounds(window);
    if (mouseAction == MouseAction::MoveVertices) {
        double dx = (cursor.x - previousMouse.x) / view.zoom, dy = (cursor.y - previousMouse.y) / view.zoom;
        bool collision = false;
        for (int v : view.selectedVertices) {
            for (std::size_t other = 0; other < view.positions.size(); ++other) if (!view.selectedVertices.count(static_cast<int>(other))) {
                const GraphPoint& p = view.positions[v]; const GraphPoint& q = view.positions[other];
                if (std::hypot(p.x + dx - q.x, p.y + dy - q.y) < (2 * view.vertexRadius + 8) / view.zoom) collision = true;
            }
        }
        if (collision) dx = dy = 0;
        for (int v : view.selectedVertices) { view.positions[v].x += dx; view.positions[v].y += dy; }
    } else if (mouseAction == MouseAction::Pan) {
        view.pan.x += cursor.x - previousMouse.x; view.pan.y += cursor.y - previousMouse.y;
    } else if (mouseAction == MouseAction::Rectangle) {
        view.selectionRectangle.right = cursor.x; view.selectionRectangle.bottom = cursor.y;
    }
    if (view.connectionStart >= 0) {
        GraphHit target = hitTestGraph(currentGraph, view, canvas, cursor);
        if (target.vertex >= 0) {
            GraphPoint center = graphToScreen(view, canvas, view.positions[target.vertex]);
            view.cursor = {static_cast<LONG>(center.x), static_cast<LONG>(center.y)};
        } else view.cursor = cursor;
    }
    previousMouse = cursor;
    if (mouseAction == MouseAction::None && PtInRect(&canvas, cursor)) {
        GraphHit hit = hitTestGraph(currentGraph, view, canvas, cursor);
        if (hit.vertex != view.hoveredVertex || hit.edge != view.hoveredEdge || hit.cluster != view.hoveredCluster) {
            view.hoveredVertex = hit.vertex; view.hoveredEdge = hit.edge; view.hoveredCluster = hit.cluster;
            InvalidateRect(window, nullptr, FALSE);
        }
        if (!trackingCanvas) {
            TRACKMOUSEEVENT track = {}; track.cbSize = sizeof(track); track.dwFlags = TME_LEAVE; track.hwndTrack = window;
            trackingCanvas = TrackMouseEvent(&track) != 0;
        }
    }
    if (mouseAction != MouseAction::None) InvalidateRect(window, nullptr, FALSE);
}
void endMouse(HWND window, POINT cursor, bool right) {
    MouseAction action = mouseAction;
    int from = view.connectionStart;
    bool moved = std::hypot(static_cast<double>(cursor.x - mouseStart.x), static_cast<double>(cursor.y - mouseStart.y)) > 4;
    if (action == MouseAction::Rectangle) {
        view.selectionRectangle.right = cursor.x; view.selectionRectangle.bottom = cursor.y;
        selectGraphRectangle(currentGraph, view, drawingBounds(window), view.selectionRectangle);
    }
    GraphHit target = hitTestGraph(currentGraph, view, drawingBounds(window), cursor);
    cancelInteraction(window);
    if (right && action == MouseAction::Connect && moved && target.vertex >= 0 && target.vertex != from) connectVertices(window, from, target.vertex);
    else if (right && !moved) contextMenu(window, cursor);
    InvalidateRect(window, nullptr, FALSE);
}

HWND control(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style, int id = 0) {
    bool edit = std::wstring(type) == L"EDIT";
    HWND handle = CreateWindowExW(edit ? WS_EX_CLIENTEDGE : 0, type, text,
        WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), TRUE);
    return handle;
}
HWND button(HWND window, const wchar_t* text, int id) { return control(window, L"BUTTON", text, WS_TABSTOP | BS_OWNERDRAW, id); }
void move(HWND handle, int x, int y, int w, int h) { MoveWindow(handle, scaled(x), scaled(y), scaled(w), scaled(h), TRUE); }
void layout(HWND window) {
    finishCellEdit(window, true); finishWeightEdit(window, true);
    RECT bounds; GetClientRect(window, &bounds);
    int width = MulDiv(bounds.right, 96, dpi), height = MulDiv(bounds.bottom, 96, dpi);
    move(samples, 40, 50, 320, 240);
    move(openButton, 40, 88, 155, 32); move(saveButton, 205, 88, 155, 32);
    move(edgeTable, 40, 180, 320, height - 563);
    move(compression, 40, height - 375, 320, 24);
    move(runButton, 40, height - 345, 205, 36); move(clearButton, 255, height - 345, 105, 36);
    move(generatorVertices, 40, height - 251, 150, 30); move(generatorEdges, 210, height - 251, 150, 30);
    move(minEdit, 40, height - 191, 150, 28); move(maxEdit, 210, height - 191, 150, 28);
    move(connected, 40, height - 157, 320, 24); move(generateButton, 40, height - 102, 320, 34);
    move(resultEdit, 406, height - 210, width - 452, 140);
    move(showWeights, 406, 32, 190, 28); move(fitButton, width - 310, 30, 100, 30);
    move(zoomOutButton, width - 200, 30, 30, 30); move(zoomInButton, width - 165, 30, 30, 30);
    move(zoomResetButton, width - 130, 30, 82, 30);
    RECT canvas = drawingBounds(window);
    move(canvasHScroll, canvas.left, canvas.bottom + 2, canvas.right - canvas.left, 14);
    move(canvasVScroll, canvas.right + 2, canvas.top, 14, canvas.bottom - canvas.top);
    fitView(window);
}
void updateCanvasScrollbars(HWND window) {
    if (!canvasHScroll || !canvasVScroll) return;
    RECT canvas = drawingBounds(window);
    double minX = 0, minY = 0, maxX = canvas.right - canvas.left, maxY = canvas.bottom - canvas.top;
    for (GraphPoint p : view.positions) {
        minX = std::min(minX, p.x * view.zoom - 80); maxX = std::max(maxX, p.x * view.zoom + 80);
        minY = std::min(minY, p.y * view.zoom - 80); maxY = std::max(maxY, p.y * view.zoom + 80);
    }
    auto configure = [&](HWND bar, double low, double high, int page, double pan) {
        SCROLLINFO info = {}; info.cbSize = sizeof(info); info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = static_cast<int>(std::floor(std::min(low - page / 2, -pan)));
        info.nMax = static_cast<int>(std::ceil(std::max(high + page / 2, -pan + page))) - 1;
        info.nPage = page; info.nPos = static_cast<int>(std::lround(-pan));
        SetScrollInfo(bar, SB_CTL, &info, TRUE);
    };
    configure(canvasHScroll, minX, maxX, canvas.right - canvas.left, view.pan.x);
    configure(canvasVScroll, minY, maxY, canvas.bottom - canvas.top, view.pan.y);
}
void paintText(HDC dc, const std::wstring& text, int x, int y, int width, int height, HFONT font, COLORREF color) {
    SelectObject(dc, font); SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
    RECT rect = {x, y, x + width, y + height};
    DrawTextW(dc, text.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
void panel(HDC dc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color); HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 16, 16);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(brush); DeleteObject(pen);
}
void paint(HWND window, HDC target) {
    RECT bounds; GetClientRect(window, &bounds);
    int pixelWidth = bounds.right, pixelHeight = bounds.bottom;
    int w = MulDiv(pixelWidth, 96, dpi), h = MulDiv(pixelHeight, 96, dpi);
    HDC dc = CreateCompatibleDC(target); HBITMAP bitmap = CreateCompatibleBitmap(target, pixelWidth, pixelHeight);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap); HGDIOBJ oldFont = SelectObject(dc, bodyFont);
    SetMapMode(dc, MM_ANISOTROPIC);
    SetWindowExtEx(dc, 96, 96, nullptr); SetViewportExtEx(dc, dpi, dpi, nullptr);
    RECT logicalBounds = {0, 0, w + 1, h + 1};
    FillRect(dc, &logicalBounds, backgroundBrush);
    panel(dc, {24, 16, 378, h - 48}, RGB(255, 255, 255));
    panel(dc, {390, 16, w - 24, h - 48}, RGB(255, 255, 255));
    paintText(dc, L"DỮ LIỆU ĐẦU VÀO", 40, 22, 320, 24, headingFont, ink);
    paintText(dc, std::to_wstring(currentGraph.vertexCount) + L" đỉnh · " + std::to_wstring(currentGraph.edges.size()) + L" cạnh", 40, 125, 320, 24, bodyFont, muted);
    paintText(dc, L"DANH SÁCH CẠNH · nhấp đúp để sửa", 40, 151, 320, 24, headingFont, ink);
    paintText(dc, L"SINH ĐỒ THỊ", 40, h - 301, 320, 24, headingFont, ink);
    paintText(dc, L"Số đỉnh", 40, h - 275, 150, 22, bodyFont, muted);
    paintText(dc, L"Số cạnh", 210, h - 275, 150, 22, bodyFont, muted);
    paintText(dc, L"Trọng số nhỏ nhất", 40, h - 215, 150, 22, bodyFont, muted);
    paintText(dc, L"Trọng số lớn nhất", 210, h - 215, 150, 22, bodyFont, muted);
    paintText(dc, generatorHint, 40, h - 130, 320, 24, bodyFont, muted);
    paintText(dc, L"KẾT QUẢ THUẬT TOÁN", 406, h - 242, w - 452, 26, headingFont, ink);
    RECT drawing = drawingBounds(window);
    updateCanvasScrollbars(window);
    Graph empty;
    drawGraphInteractive(dc, drawing, graphReady ? currentGraph : empty, resultReady ? &currentResult : nullptr, headingFont, view);
    if (currentGraph.vertexCount > GuiLimits::DRAW_VERTICES || currentGraph.edges.size() > GuiLimits::DRAW_EDGES) {
        std::wstring summary = L"Bản xem rút gọn: " + std::to_wstring(view.positions.size()) + L"/" + std::to_wstring(currentGraph.vertexCount)
            + L" đỉnh (từ 0), " + std::to_wstring(view.visibleEdges.size()) + L"/" + std::to_wstring(currentGraph.edges.size()) + L" cạnh. Kết quả tính toàn bộ.";
        paintText(dc, summary, 406, h - 295, w - 452, 22, bodyFont, muted);
    } else {
        const auto groups = graphClusters(view, drawing);
        bool grouped = false;
        for (const auto& group : groups) if (group.vertices.size() > 1) grouped = true;
        if (grouped) paintText(dc, L"Tổng quan theo nhóm · Bấm vào nhóm để xem trọng số từng cạnh.", 406, h - 295, w - 452, 22, bodyFont, muted);
    }
    paintText(dc, L"Xanh: MST · Đỏ: đang chọn · Nhấp đúp đỉnh để đổi tên", 406, h - 270, w - 452, 22, bodyFont, muted);
    paintText(dc, status, 26, h - 38, w - 52, 28, bodyFont, muted);
    SetMapMode(dc, MM_TEXT);
    BitBlt(target, 0, 0, pixelWidth, pixelHeight, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldFont); SelectObject(dc, oldBitmap); DeleteObject(bitmap); DeleteDC(dc);
}
void drawButton(const DRAWITEMSTRUCT* item) {
    bool primary = item->CtlID == RUN;
    bool pressed = (item->itemState & ODS_SELECTED) != 0;
    COLORREF fill = primary ? (pressed ? RGB(29, 78, 216) : accent) : (pressed ? RGB(226, 232, 240) : RGB(241, 245, 249));
    panel(item->hDC, item->rcItem, fill);
    std::wstring text = textOf(item->hwndItem);
    SelectObject(item->hDC, controlHeadingFont); SetTextColor(item->hDC, item->itemState & ODS_DISABLED ? muted : primary ? RGB(255, 255, 255) : ink); SetBkMode(item->hDC, TRANSPARENT);
    RECT rect = item->rcItem; DrawTextW(item->hDC, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (item->itemState & ODS_FOCUS) { InflateRect(&rect, -4, -4); DrawFocusRect(item->hDC, &rect); }
}

// A small native table: the core still receives the same Graph/Edge structures.
// A separate text mode supports pasting several edges at once.
void refreshTable() {
    if (!edgeTable) return;
    RECT r; GetClientRect(edgeTable, &r);
    int page = std::max(1, (MulDiv(r.bottom, 96, dpi) - 28) / 26);
    tableTopRow = std::max(0, std::min(tableTopRow, static_cast<int>(currentGraph.edges.size()) - page));
    SCROLLINFO scroll = {}; scroll.cbSize = sizeof(scroll); scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMax = std::max(0, static_cast<int>(currentGraph.edges.size()) - 1);
    scroll.nPage = page; scroll.nPos = tableTopRow; SetScrollInfo(edgeTable, SB_VERT, &scroll, TRUE);
    InvalidateRect(edgeTable, nullptr, FALSE);
}
void finishCellEdit(HWND window, bool save) {
    if (cellRow < 0) return;
    int row = cellRow, column = cellColumn; cellRow = -1; cellColumn = -1;
    if (save && row < static_cast<int>(currentGraph.edges.size())) {
        try {
            Graph next = currentGraph;
            Edge& edge = next.edges[row];
            if (column < 2) {
                int vertex = vertexIndex(view.labels, currentGraph.vertexCount, textOf(tableCellEditor));
                if (column == 0) edge.from = vertex; else edge.to = vertex;
            } else {
                long long value = field(tableCellEditor);
                if (value < INT_MIN || value > INT_MAX) throw std::invalid_argument("Trọng số phải là số nguyên 32-bit.");
                edge.weight = static_cast<int>(value);
            }
            GuiLimits::validate(next);
            currentGraph = next;
            if (view.positions.size() != static_cast<std::size_t>(std::min(next.vertexCount, GuiLimits::DRAW_VERTICES))) resetView(window);
            graphChanged(window);
        } catch (const std::exception& error) {
            status = localizedError(error); KillTimer(window, STATUS_TIMER);
            InvalidateRect(window, nullptr, FALSE);
        }
    }
    ShowWindow(tableCellEditor, SW_HIDE);
}
LRESULT CALLBACK cellProc(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
    HWND window = GetParent(edit);
    if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if (message == WM_KEYDOWN && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        finishCellEdit(window, wParam == VK_RETURN); SetFocus(edgeTable); return 0;
    }
    if (message == WM_KILLFOCUS) finishCellEdit(window, true);
    return CallWindowProcW(originalCellProc, edit, message, wParam, lParam);
}
void beginCellEdit(HWND window, int row, int column) {
    if (workerBusy) return;
    finishCellEdit(window, true); finishWeightEdit(window, true);
    if (row < 0 || row >= static_cast<int>(currentGraph.edges.size()) || column < 0 || column > 2) return;
    int columns[] = {36, 126, 216, 320};
    const Edge& edge = currentGraph.edges[row];
    int value = column == 0 ? edge.from : column == 1 ? edge.to : edge.weight;
    RECT tableRect; GetWindowRect(edgeTable, &tableRect);
    POINT origin = {tableRect.left, tableRect.top}; ScreenToClient(window, &origin);
    MoveWindow(tableCellEditor, origin.x + scaled(columns[column]), origin.y + scaled(28 + (row - tableTopRow) * 26),
        scaled(columns[column + 1] - columns[column]), scaled(27), TRUE);
    SetWindowTextW(tableCellEditor, (column < 2 ? vertexLabel(view.labels, value) : std::to_wstring(value)).c_str());
    cellRow = row; cellColumn = column; ShowWindow(tableCellEditor, SW_SHOW);
    SetFocus(tableCellEditor); SendMessageW(tableCellEditor, EM_SETSEL, 0, -1);
}
LRESULT CALLBACK tableProc(HWND table, UINT message, WPARAM wParam, LPARAM lParam) {
    HWND window = GetParent(table);
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        PAINTSTRUCT ps = {}; HDC dc = message == WM_PAINT ? BeginPaint(table, &ps) : reinterpret_cast<HDC>(wParam);
        int saved = SaveDC(dc); RECT rect; GetClientRect(table, &rect);
        FillRect(dc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SelectObject(dc, controlFont); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, ink);
        int columns[] = {0, scaled(36), scaled(126), scaled(216), rect.right};
        const wchar_t* headers[] = {L"STT", L"Đầu (u)", L"Cuối (v)", L"Trọng số"};
        HBRUSH headerBrush = CreateSolidBrush(RGB(235, 240, 247));
        RECT header = {0, 0, rect.right, scaled(28)}; FillRect(dc, &header, headerBrush); DeleteObject(headerBrush);
        for (int c = 0; c < 4; ++c) {
            RECT cell = {columns[c], 0, columns[c + 1], scaled(28)};
            DrawTextW(dc, headers[c], -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(227, 232, 240)); HGDIOBJ oldPen = SelectObject(dc, pen);
        for (int row = tableTopRow, y = scaled(28); row < static_cast<int>(currentGraph.edges.size()) && y < rect.bottom; ++row, y += scaled(26)) {
            RECT line = {0, y, rect.right, y + scaled(26)};
            COLORREF fill = view.selectedEdges.count(row) ? RGB(219, 234, 254) : row % 2 ? RGB(247, 249, 252) : RGB(255, 255, 255);
            HBRUSH brush = CreateSolidBrush(fill); FillRect(dc, &line, brush); DeleteObject(brush);
            const Edge& edge = currentGraph.edges[row]; int values[] = {row + 1, edge.from, edge.to, edge.weight};
            for (int c = 0; c < 4; ++c) {
                RECT cell = {columns[c], y, columns[c + 1], y + scaled(26)};
                std::wstring text = c == 1 || c == 2 ? vertexLabel(view.labels, values[c]) : std::to_wstring(values[c]);
                DrawTextW(dc, text.c_str(), -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            MoveToEx(dc, 0, y + scaled(26), nullptr); LineTo(dc, rect.right, y + scaled(26));
        }
        for (int c = 1; c < 4; ++c) { MoveToEx(dc, columns[c], 0, nullptr); LineTo(dc, columns[c], rect.bottom); }
        SelectObject(dc, oldPen); DeleteObject(pen); RestoreDC(dc, saved);
        if (message == WM_PAINT) EndPaint(table, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        int x = MulDiv(GET_X_LPARAM(lParam), 96, dpi), y = MulDiv(GET_Y_LPARAM(lParam), 96, dpi);
        if (y < 28) return 0;
        int row = tableTopRow + (y - 28) / 26;
        if (row >= static_cast<int>(currentGraph.edges.size())) return 0;
        view.selectedVertices.clear(); view.selectedEdges.clear(); view.selectedEdges.insert(row);
        SetFocus(table); InvalidateRect(table, nullptr, FALSE); InvalidateRect(window, nullptr, FALSE);
        if (message == WM_LBUTTONDBLCLK) beginCellEdit(window, row, x < 36 ? -1 : x < 126 ? 0 : x < 216 ? 1 : 2);
        return 0;
    }
    case WM_VSCROLL:
    case WM_MOUSEWHEEL: {
        finishCellEdit(window, true);
        if (message == WM_MOUSEWHEEL) tableTopRow -= GET_WHEEL_DELTA_WPARAM(wParam) / 120 * 3;
        else {
            SCROLLINFO info = {}; info.cbSize = sizeof(info); info.fMask = SIF_ALL; GetScrollInfo(table, SB_VERT, &info);
            switch (LOWORD(wParam)) {
            case SB_LINEUP: --tableTopRow; break; case SB_LINEDOWN: ++tableTopRow; break;
            case SB_PAGEUP: tableTopRow -= info.nPage; break; case SB_PAGEDOWN: tableTopRow += info.nPage; break;
            case SB_THUMBTRACK: tableTopRow = info.nTrackPos; break;
            }
        }
        refreshTable(); return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_DELETE || wParam == VK_BACK || wParam == VK_ESCAPE) { SendMessageW(window, message, wParam, lParam); return 0; }
        break;
    case WM_SIZE: refreshTable(); return 0;
    }
    return DefWindowProcW(table, message, wParam, lParam);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        WNDCLASSW tableClass = {}; tableClass.lpfnWndProc = tableProc;
        tableClass.hInstance = GetModuleHandleW(nullptr); tableClass.lpszClassName = L"KruskalEdgeTable";
        tableClass.style = CS_DBLCLKS; tableClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&tableClass);
        whiteBrush = CreateSolidBrush(RGB(255, 255, 255)); backgroundBrush = CreateSolidBrush(background);
        bodyFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        headingFont = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        updateControlFonts(window);
        samples = control(window, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, SAMPLES);
        DWORD attributes = GetFileAttributesW(samplePath(6).c_str());
        sampleCount = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) ? 7 : 6;
        for (int i = 0; i < sampleCount; ++i) SendMessageW(samples, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sampleNames[i]));
        std::wstring blank = L"0" + std::to_wstring(sampleCount + 1) + L"  Bảng trắng";
        SendMessageW(samples, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(blank.c_str()));
        SendMessageW(samples, CB_SETCURSEL, 0, 0);
        openButton = button(window, L"Mở file .txt", OPEN); saveButton = button(window, L"Lưu đồ thị", SAVE);
        resultEdit = control(window, L"EDIT", L"", WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL);
        SendMessageW(resultEdit, EM_SETLIMITTEXT, 2 * 1024 * 1024, 0);
        compression = control(window, L"BUTTON", L"Bật path compression", WS_TABSTOP | BS_AUTOCHECKBOX);
        connected = control(window, L"BUTTON", L"Bảo đảm đồ thị liên thông", WS_TABSTOP | BS_AUTOCHECKBOX);
        SendMessageW(compression, BM_SETCHECK, BST_CHECKED, 0); SendMessageW(connected, BM_SETCHECK, BST_CHECKED, 0);
        minEdit = control(window, L"EDIT", L"1", WS_TABSTOP | ES_AUTOHSCROLL);
        maxEdit = control(window, L"EDIT", L"20", WS_TABSTOP | ES_AUTOHSCROLL);
        generatorVertices = control(window, L"EDIT", L"10", WS_TABSTOP | ES_AUTOHSCROLL);
        generatorEdges = control(window, L"EDIT", L"20", WS_TABSTOP | ES_AUTOHSCROLL);
        for (HWND field : {minEdit, maxEdit, generatorVertices, generatorEdges}) SendMessageW(field, EM_SETLIMITTEXT, 11, 0);
        runButton = button(window, L"Chạy Kruskal", RUN); clearButton = button(window, L"Xóa dữ liệu", CLEAR);
        generateButton = button(window, L"Sinh đồ thị ngẫu nhiên", GENERATE);
        edgeTable = control(window, L"KruskalEdgeTable", L"", WS_BORDER | WS_TABSTOP | WS_VSCROLL);
        tableCellEditor = control(window, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL);
        originalCellProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(tableCellEditor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(cellProc)));
        ShowWindow(tableCellEditor, SW_HIDE);
        showWeights = control(window, L"BUTTON", L"Hiện trọng số", WS_TABSTOP | BS_AUTOCHECKBOX, SHOW_WEIGHTS);
        SendMessageW(showWeights, BM_SETCHECK, BST_CHECKED, 0);
        fitButton = button(window, L"Vừa khung", FIT_VIEW);
        canvasHScroll = control(window, L"SCROLLBAR", L"", SBS_HORZ);
        canvasVScroll = control(window, L"SCROLLBAR", L"", SBS_VERT);
        zoomOutButton = button(window, L"-", ZOOM_OUT); zoomInButton = button(window, L"+", ZOOM_IN);
        zoomResetButton = button(window, L"100%", ZOOM_RESET);
        weightEditor = control(window, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL);
        SendMessageW(weightEditor, EM_SETLIMITTEXT, 16, 0);
        originalWeightProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(weightEditor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(weightProc)));
        ShowWindow(weightEditor, SW_HIDE);
        layout(window);
        try { loadSample(window); } catch (const std::exception& error) { showError(window, error); }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam); info->ptMinTrackSize = {scaled(1120), scaled(900)}; return 0;
    }
    case WM_DPICHANGED: {
        dpi = HIWORD(wParam);
        updateControlFonts(window);
        RECT* rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        layout(window); return 0;
    }
    case WM_SIZE: if (resultEdit) layout(window); return 0;
    case WM_HSCROLL:
    case WM_VSCROLL: {
        HWND bar = reinterpret_cast<HWND>(lParam);
        if (bar != canvasHScroll && bar != canvasVScroll) break;
        finishWeightEdit(window, true); cancelInteraction(window);
        SCROLLINFO info = {}; info.cbSize = sizeof(info); info.fMask = SIF_ALL; GetScrollInfo(bar, SB_CTL, &info);
        int position = info.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINELEFT: position -= 32; break; case SB_LINERIGHT: position += 32; break;
        case SB_PAGELEFT: position -= info.nPage / 2; break; case SB_PAGERIGHT: position += info.nPage / 2; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: position = info.nTrackPos; break;
        case SB_LEFT: position = info.nMin; break; case SB_RIGHT: position = info.nMax; break;
        default: return 0;
        }
        position = std::max(info.nMin, std::min(position, info.nMax - static_cast<int>(info.nPage) + 1));
        if (bar == canvasHScroll) view.pan.x = -position; else view.pan.y = -position;
        updateCanvasScrollbars(window); InvalidateRect(window, nullptr, FALSE); return 0;
    }
    case WORK_DONE: {
        std::unique_ptr<WorkResult> task(reinterpret_cast<WorkResult*>(lParam));
        if (worker.joinable()) worker.join();
        setBusy(window, false);
        if (closeWhenDone) { DestroyWindow(window); return 0; }
        try {
            if (!task->error.empty()) throw std::runtime_error(task->error);
            if (task->kind == 0) presentResult(window, task->graph, task->result, task->milliseconds);
            else if (task->kind == 3) { status = L"Đã lưu toàn bộ dữ liệu."; SetTimer(window, STATUS_TIMER, 5000, nullptr); }
            else putGraph(window, task->graph, task->kind == 1 ? L"Đồ thị ngẫu nhiên" : task->name, true, task->kind == 2 ? task->labels : VertexLabels());
        } catch (const std::exception& error) { showError(window, error); }
        return 0;
    }
    case WM_CLOSE:
        if (workerBusy) { closeWhenDone = true; status = L"Đang hoàn tất tác vụ trước khi đóng..."; InvalidateRect(window, nullptr, FALSE); return 0; }
        DestroyWindow(window); return 0;
    case WM_TIMER:
        if (wParam == STATUS_TIMER) { status.clear(); KillTimer(window, STATUS_TIMER); InvalidateRect(window, nullptr, FALSE); return 0; }
        break;
    case WM_LBUTTONDOWN:
        try { leftDown(window, mousePoint(lParam), (wParam & MK_CONTROL) != 0); }
        catch (const std::exception& error) { showError(window, error); }
        return 0;
    case WM_LBUTTONDBLCLK: {
        POINT cursor = mousePoint(lParam);
        GraphHit hit = hitTestGraph(currentGraph, view, drawingBounds(window), cursor);
        if (hit.vertex >= 0) {
            try { beginVertexEdit(window, hit.vertex); } catch (const std::exception& error) { showError(window, error); }
        } else if (hit.edge >= 0) {
            try { beginWeightEdit(window, hit.edge); }
            catch (const std::exception& error) { showError(window, error); }
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (mouseAction != MouseAction::None && mouseAction != MouseAction::Connect) endMouse(window, mousePoint(lParam), false);
        return 0;
    case WM_RBUTTONDOWN: {
        POINT cursor = mousePoint(lParam); RECT canvas = drawingBounds(window);
        if (!PtInRect(&canvas, cursor)) return 0;
        try {
            finishWeightEdit(window, true); requireEditable(); cancelInteraction(window); SetFocus(window);
            GraphHit hit = hitTestGraph(currentGraph, view, canvas, cursor);
            mouseStart = previousMouse = cursor;
            if (hit.vertex >= 0) { mouseAction = MouseAction::Connect; view.connectionStart = hit.vertex; view.cursor = cursor; }
            SetCapture(window);
        } catch (const std::exception& error) { showError(window, error); }
        return 0;
    }
    case WM_RBUTTONUP:
        try { endMouse(window, mousePoint(lParam), true); }
        catch (const std::exception& error) { cancelInteraction(window); showError(window, error); }
        return 0;
    case WM_MBUTTONDOWN: {
        POINT cursor = mousePoint(lParam); RECT canvas = drawingBounds(window);
        if (PtInRect(&canvas, cursor)) {
            finishWeightEdit(window, true); cancelInteraction(window); SetFocus(window);
            mouseStart = previousMouse = cursor; mouseAction = MouseAction::Pan; SetCapture(window);
        }
        return 0;
    }
    case WM_MBUTTONUP:
        if (mouseAction == MouseAction::Pan) { cancelInteraction(window); InvalidateRect(window, nullptr, FALSE); }
        return 0;
    case WM_MOUSEMOVE: mouseMove(window, mousePoint(lParam)); return 0;
    case WM_MOUSELEAVE:
        trackingCanvas = false; view.hoveredVertex = -1; view.hoveredEdge = -1; view.hoveredCluster = -1;
        InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_MOUSEWHEEL: {
        POINT pixel = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(window, &pixel);
        POINT cursor = {MulDiv(pixel.x, 96, dpi), MulDiv(pixel.y, 96, dpi)};
        RECT canvas = drawingBounds(window);
        if (PtInRect(&canvas, cursor)) {
            finishWeightEdit(window, true);
            zoomGraph(view, canvas, std::pow(1.15, GET_WHEEL_DELTA_WPARAM(wParam) / 120.0), GraphPoint(cursor.x, cursor.y));
            SetWindowTextW(zoomResetButton, (std::to_wstring(static_cast<int>(std::lround(view.zoom * 100))) + L"%").c_str());
            InvalidateRect(window, nullptr, FALSE); return 0;
        }
        break;
    }
    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) != window) {
            mouseAction = MouseAction::None; view.connectionStart = -1; view.selectingRectangle = false;
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_CANCELMODE: cancelInteraction(window); InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_CONTEXTMENU: {
        if (reinterpret_cast<HWND>(wParam) != window) break;
        POINT pixel = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (pixel.x == -1 && pixel.y == -1) {
            RECT canvas = drawingBounds(window); pixel = {scaled((canvas.left + canvas.right) / 2), scaled((canvas.top + canvas.bottom) / 2)};
        } else ScreenToClient(window, &pixel);
        POINT cursor = {MulDiv(pixel.x, 96, dpi), MulDiv(pixel.y, 96, dpi)};
        try { contextMenu(window, cursor); } catch (const std::exception& error) { showError(window, error); }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            finishWeightEdit(window, false); cancelInteraction(window);
            view.selectedVertices.clear(); view.selectedEdges.clear(); InvalidateRect(window, nullptr, FALSE); return 0;
        }
        if (wParam == VK_DELETE || wParam == VK_BACK) {
            try { deleteSelection(window); } catch (const std::exception& error) { showError(window, error); }
            return 0;
        }
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(window, &ps); paint(window, dc); EndPaint(window, &ps); return 0; }
    case WM_DRAWITEM: drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(wParam), ink); SetBkColor(reinterpret_cast<HDC>(wParam), RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(whiteBrush);
    case WM_COMMAND:
        if ((reinterpret_cast<HWND>(lParam) == minEdit || reinterpret_cast<HWND>(lParam) == maxEdit ||
             reinterpret_cast<HWND>(lParam) == generatorVertices || reinterpret_cast<HWND>(lParam) == generatorEdges) && HIWORD(wParam) == EN_CHANGE) {
            if (resultEdit) updateHints(window);
            return 0;
        }
        try {
            if (LOWORD(wParam) == SAMPLES && HIWORD(wParam) == CBN_SELCHANGE) { finishWeightEdit(window, false); loadSample(window); }
            else if (HIWORD(wParam) == BN_CLICKED) switch (LOWORD(wParam)) {
            case FIT_VIEW: fitView(window); break;
            case SHOW_WEIGHTS:
                view.showAllWeights = SendMessageW(showWeights, BM_GETCHECK, 0, 0) == BST_CHECKED;
                InvalidateRect(window, nullptr, FALSE); break;
            case RUN: finishWeightEdit(window, true); cancelInteraction(window); runKruskal(window); break;
            case ADD_VERTEX: addVertexAt(window); break;
            case EDIT_EDGE:
                requireEditable();
                if (view.selectedEdges.size() != 1) throw std::invalid_argument("Vui lòng chọn đúng một cạnh trước khi sửa trọng số.");
                beginWeightEdit(window, *view.selectedEdges.begin()); break;
            case DELETE_SELECTED: deleteSelection(window); break;
            case ZOOM_OUT: adjustZoom(window, 1 / 1.15); break;
            case ZOOM_IN: adjustZoom(window, 1.15); break;
            case ZOOM_RESET:
                finishWeightEdit(window, true); view.zoom = 1; view.pan = GraphPoint(); SetWindowTextW(zoomResetButton, L"100%"); InvalidateRect(window, nullptr, FALSE); break;
            case GENERATE: generate(window); break;
            case SAVE: saveGraph(window); break;
            case OPEN: {
                std::wstring path;
                if (chooseFile(window, false, path)) startWork(window, 2, Graph(), GeneratorOptions(), false, path);
                break;
            }
            case CLEAR:
                clearGraph(window); SetFocus(window); break;
            }
            if (reinterpret_cast<HWND>(lParam) == connected) updateHints(window);
            if (!status.empty() && !workerBusy) SetTimer(window, STATUS_TIMER, 5000, nullptr);
        } catch (const std::exception& error) { showError(window, error); }
        return 0;
    case WM_DESTROY:
        if (worker.joinable()) worker.join();
        { MSG pending; while (PeekMessageW(&pending, window, WORK_DONE, WORK_DONE, PM_REMOVE)) delete reinterpret_cast<WorkResult*>(pending.lParam); }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR graphicsToken;
    if (Gdiplus::GdiplusStartup(&graphicsToken, &startup, nullptr) != Gdiplus::Ok) return 1;
    // Disable bitmap enlargement by Windows; redraw text/layout at monitor DPI.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) SetProcessDPIAware();
    HDC screen = GetDC(nullptr); dpi = GetDeviceCaps(screen, LOGPIXELSX); ReleaseDC(nullptr, screen);
    WNDCLASSW wc = {}; wc.lpfnWndProc = windowProc; wc.hInstance = instance;
    wc.lpszClassName = L"DSAKruskalStudio"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.style = CS_DBLCLKS;
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowW(wc.lpszClassName, L"Kruskal Studio", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, scaled(1280), scaled(980), nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message; int code;
    while ((code = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (message.message == WM_KEYDOWN && message.hwnd == window &&
            (message.wParam == VK_ESCAPE || message.wParam == VK_DELETE || message.wParam == VK_BACK)) {
            SendMessageW(window, message.message, message.wParam, message.lParam); continue;
        }
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    for (HFONT font : {bodyFont, headingFont}) DeleteObject(font);
    DeleteObject(controlFont); DeleteObject(controlHeadingFont);
    DeleteObject(whiteBrush); DeleteObject(backgroundBrush);
    Gdiplus::GdiplusShutdown(graphicsToken);
    return code == -1 ? 1 : static_cast<int>(message.wParam);
}
