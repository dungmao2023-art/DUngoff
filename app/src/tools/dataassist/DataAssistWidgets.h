// SPDX-License-Identifier: MPL-2.0
// Giao diện tối thiểu cho các tiện ích dữ liệu (đăng ký vào Hộp công cụ).
#pragma once
#include <QWidget>
#include <functional>

namespace dataassist {

// Móc nối tuỳ chọn do nơi nhúng cung cấp. Chưa có móc => widget chỉ sao chép vào bảng nhớ tạm và nói rõ.
struct Hooks {
    std::function<bool(const QString &formula)> insertFormula;   // đưa công thức vào ô đang chọn của tab Excel (engine tính)
    std::function<QString()> selectedRangeText;                    // vùng ô đang chọn dạng TSV (selectionAs text/plain)
};
Hooks &hooks();

}  // namespace dataassist

QWidget* createFormulaAssistantWidget(QWidget *parent = nullptr);   // ƒx · Tôi muốn
QWidget* createRangeToolsWidget(QWidget *parent = nullptr);         // Thống kê vùng · Kiểm tra lỗi · Biểu đồ nhanh (dán/CSV/xlsx)
QWidget* createTemplateExportWidget(QWidget *parent = nullptr);     // Xuất theo mẫu MISA/FAST/KiotViet/Sapo/khác
QWidget* createLawLinksWidget(QWidget *parent = nullptr);           // Kho luật (liên kết nguồn)
