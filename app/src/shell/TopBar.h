// SPDX-License-Identifier: MPL-2.0
// Thanh trên: nhóm thẻ ứng dụng ở giữa (Trang chủ/Word/Excel/PowerPoint/PDF), nút tìm nhanh (Ctrl+K),
// trạng thái lưu; bên dưới là hàng liên kết nhanh theo ngữ cảnh.
#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

namespace shell {

class TopBar : public QWidget {
    Q_OBJECT
public:
    explicit TopBar(QWidget* parent = nullptr);
    // kind: "home", "word", "excel", "ppt", "pdf" hoặc rỗng (không thẻ nào sáng, vd. trang Công cụ)
    void setActive(const QString& kind);
    void setSaveText(const QString& html);   // vd. "Đã lưu trên máy"
    void setReadyText(const QString& text);  // góc phải hàng liên kết: "Sẵn sàng"
    void setCompact(bool compact);           // cửa sổ hẹp: gọn các thẻ

signals:
    void tabClicked(const QString& kind);
    void searchRequested();
    void quickLinkClicked(const QString& id);

private:
    QPushButton* m_tabs[5] = {};
    QLabel* m_save;
    QLabel* m_ready;
    QWidget* m_links;
};

} // namespace shell
