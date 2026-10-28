#ifndef VERTEX_LABELS_H
#define VERTEX_LABELS_H

#include <map>
#include <string>

using VertexLabels = std::map<int, std::wstring>;
std::wstring vertexLabel(const VertexLabels& labels, int vertex);
void renameVertex(VertexLabels& labels, int vertexCount, int vertex, const std::wstring& name);
int vertexIndex(const VertexLabels& labels, int vertexCount, const std::wstring& text);
// Optional UTF-8 comment lines preserve names while Graph keeps integer IDs.
VertexLabels readVertexLabels(std::string& source, int vertexCount);
std::string writeVertexLabels(const VertexLabels& labels);

#endif
