// SPDX-License-Identifier: MPL-2.0
// Giao diện tối thiểu cho các tiện ích dữ liệu (đăng ký vào Hộp công cụ).
#pragma once
#include <QWidget>
#include <functional>

namespace dataassist {

// Móc nối tuỳ chọn do nơi nhúng cung cấp. Chưa có móc => widget chỉ sao chép vào bảng nhớ tạm và nói rõ.
struct Hooks {
    // đưa công thức vào ô đang chọn của tab Excel (engine tính). false + message nếu không ghi; resultText = kết quả engine tính.
    std::function<bool(const QString &formula, QString *resultText, QString *message)> insertFormula;
    std::function<bool(const QString &target, const QString &formula, QString *resultText, QString *message)> insertFormulaAt;
    std::function<QString()> selectedCellAddress;                  // địa chỉ một ô do engine báo
    std::function<QString()> selectedRangeText;                    // vùng ô đang chọn dạng TSV (selectionAs text/plain)
};
Hooks &hooks();

}  // namespace dataassist

QWidget* createFormulaAssistantWidget(QWidget *parent = nullptr);
void notifyFormulaSelection(QWidget* widget, const QString& reference);   // ƒx · Tôi muốn
void notifyFormulaSheet(QWidget* widget, int part, const QString& name);
QWidget* createRangeToolsWidget(QWidget *parent = nullptr);
// Embedded in one document: scoped read hook and event-driven refresh, never a global polling timer.
QWidget* createDocumentRangeToolsWidget(QWidget* parent, std::function<QString()> readSelection,
                                       std::function<bool()> inputIdle = {});
void notifyDocumentRangeTools(QWidget* widget);         // Thống kê vùng · Kiểm tra lỗi · Biểu đồ nhanh (dán/CSV/xlsx)
QWidget* createTemplateExportWidget(QWidget *parent = nullptr);     // Xuất theo mẫu MISA/FAST/KiotViet/Sapo/khác
QWidget* createLawLinksWidget(QWidget *parent = nullptr);           // Kho luật (liên kết nguồn)
