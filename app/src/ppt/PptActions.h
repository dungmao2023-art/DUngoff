#pragma once
#include <QString>
class DocumentTab;

// Xử lý các hành động ứng dụng "ppt.*" của ribbon PowerPoint.
// Trả về true nếu id thuộc PowerPoint (đã xử lý).
bool handlePptAction(DocumentTab* tab, const QString& id);
