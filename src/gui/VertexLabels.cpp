#define NOMINMAX
#include "VertexLabels.h"
#include <windows.h>
#include <sstream>
#include <stdexcept>
#include <cwctype>
#include <set>

namespace {
std::wstring trimmed(const std::wstring& text) {
    const auto first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    return text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
}
bool numeric(const std::wstring& name) {
    return !name.empty() && name.find_first_not_of(L"0123456789") == std::wstring::npos;
}
std::wstring decode(const std::string& text) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n) throw std::invalid_argument("Tên đỉnh phải là UTF-8 hợp lệ.");
    std::wstring result(n, L' ');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), &result[0], n);
    return result;
}
std::string encode(const std::wstring& text) {
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::invalid_argument("Tên đỉnh chứa ký tự không hợp lệ.");
    std::string result(n, ' ');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), &result[0], n, nullptr, nullptr);
    return result;
}
}
std::wstring vertexLabel(const VertexLabels& labels, int vertex) {
    auto found = labels.find(vertex);
    return found == labels.end() ? std::to_wstring(vertex) : found->second;
}
void renameVertex(VertexLabels& labels, int vertexCount, int vertex, const std::wstring& text) {
    if (vertex < 0 || vertex >= vertexCount) throw std::invalid_argument("Đỉnh không tồn tại.");
    std::wstring name = trimmed(text);
    if (name.empty() || name.size() > 16) throw std::invalid_argument("Tên đỉnh cần từ 1 đến 16 ký tự.");
    for (wchar_t ch : name) if (iswcntrl(ch)) throw std::invalid_argument("Tên đỉnh không được chứa ký tự điều khiển.");
    encode(name);
    if (numeric(name) && name != std::to_wstring(vertex))
        throw std::invalid_argument("Tên dạng số phải đúng chỉ số gốc của đỉnh; hãy dùng chữ để đặt tên khác.");
    for (const auto& entry : labels) if (entry.first != vertex && entry.second == name)
        throw std::invalid_argument("Tên đỉnh đã được sử dụng.");
    if (name == std::to_wstring(vertex)) labels.erase(vertex);
    else labels[vertex] = name;
}
int vertexIndex(const VertexLabels& labels, int vertexCount, const std::wstring& text) {
    std::wstring name = trimmed(text);
    for (const auto& entry : labels) if (entry.second == name) return entry.first;
    if (numeric(name)) {
        try { long long id = std::stoll(name); if (id < vertexCount) return static_cast<int>(id); } catch (const std::exception&) {}
    }
    throw std::invalid_argument("Không tìm thấy đỉnh. Nhập tên hiện có hoặc chỉ số gốc của đỉnh.");
}
VertexLabels readVertexLabels(std::string& source, int vertexCount) {
    VertexLabels labels; std::set<int> ids; std::set<std::wstring> names;
    std::istringstream input(source); std::ostringstream data; std::string line;
    while (std::getline(input, line)) {
        if (line.compare(0, 7, "# name ") == 0) {
            std::istringstream row(line.substr(7)); int id; std::string name;
            if (!(row >> id) || !ids.insert(id).second) throw std::invalid_argument("Dòng tên đỉnh không hợp lệ hoặc trùng chỉ số.");
            std::getline(row >> std::ws, name);
            VertexLabels single; renameVertex(single, vertexCount, id, decode(name));
            if (!single.empty()) {
                const std::wstring& value = single.begin()->second;
                if (!names.insert(value).second) throw std::invalid_argument("Tên đỉnh đã được sử dụng.");
                labels[id] = value;
            }
        } else data << line << '\n';
    }
    source = data.str(); return labels;
}
std::string writeVertexLabels(const VertexLabels& labels) {
    std::ostringstream result;
    for (const auto& entry : labels) result << "# name " << entry.first << ' ' << encode(entry.second) << '\n';
    return result.str();
}
