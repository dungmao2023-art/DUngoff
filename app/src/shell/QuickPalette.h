// SPDX-License-Identifier: MPL-2.0
// Bảng lệnh nhanh (Ctrl+K): gõ để lọc danh sách hành động THẬT của ứng dụng, không dấu cũng tìm được.
#pragma once
#include <QDialog>
#include <QList>

class QLineEdit;
class QListWidget;

namespace shell {

struct PaletteEntry {
    QString id;     // mã hành động, MainWindow xử lý
    QString title;
    QString hint;   // mô tả ngắn / phím tắt
};

// Bỏ dấu tiếng Việt và hạ chữ thường để so khớp ("Mở tệp" khớp "mo tep").
QString foldVietnamese(const QString& s);

class QuickPalette : public QDialog {
    Q_OBJECT
public:
    explicit QuickPalette(QWidget* parent = nullptr);
    void setEntries(const QList<PaletteEntry>& entries);
    void popup(); // đặt giữa cửa sổ cha, xoá ô tìm, focus

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

signals:
    void triggered(const QString& id);

private:
    void refilter();
    QLineEdit* m_edit;
    QListWidget* m_list;
    QList<PaletteEntry> m_entries;
};

} // namespace shell
