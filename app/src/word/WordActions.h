// SPDX-License-Identifier: MPL-2.0
// Hành động Word cần hộp thoại (phông, cỡ chữ, màu, bảng, ảnh, liên kết, tìm/thay, khổ giấy, đếm từ).
// Id có dạng "word:<tên>". Trả true nếu id thuộc Word (đã xử lý hoặc bỏ qua vì chỉ đọc).
#pragma once
#include <QString>

class DocumentTab;
class QWidget;

namespace dung::word {
bool handleAction(const QString& id, DocumentTab* tab, QWidget* parent);
}
