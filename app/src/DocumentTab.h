// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "dung/Engine.h"

#include <QHash>
#include <QWidget>
#include <memory>

class QLineEdit;
class QTabBar;
class QListWidget;
class TileView;

class DocumentTab : public QWidget {
    Q_OBJECT
public:
    DocumentTab(std::unique_ptr<dung::Document> doc, QWidget* parent = nullptr);
    ~DocumentTab() override;

    dung::Document* document() const { return m_doc.get(); }
    dung::DocKind kind() const { return m_doc->kind(); }
    bool readOnly() const { return m_doc->readOnly(); }
    QString title() const;
    QString statusText() const;
    const QHash<QString, QString>& states() const { return m_states; }
    TileView* view() const { return m_view; }

    void runCommand(const QString& uno);
    void copy();
    void cut();
    void paste();
    bool save();            // Ctrl+S: lưu vào tệp mới đã chọn trước đó, hoặc hỏi Lưu thành
    bool saveAs();          // luôn hỏi đường dẫn
    bool exportPdf();
    bool maybeClose();      // hỏi lưu nếu có thay đổi; false = huỷ đóng
    void setZoom(int percent);
    int zoom() const;
    void stepPart(int delta);  // chuyển trang/slide trước (-1) hoặc sau (+1)

signals:
    void titleChanged();
    void statusChanged();
    void commandStateChanged(const QString& cmd, const QString& value);
    void savedTo(const QString& path);

private:
    void onEngineEvent(int type, const QString& payload);
    void rebuildParts();
    bool doSave(const QString& path, bool confirmedReplace);

    std::unique_ptr<dung::Document> m_doc;
    TileView* m_view = nullptr;
    QLineEdit* m_nameBox = nullptr;
    QLineEdit* m_formula = nullptr;
    QTabBar* m_sheets = nullptr;
    QListWidget* m_slides = nullptr;
    QString m_savedPath;    // tệp mới mà người dùng đã "Lưu thành" trong phiên này
    QString m_cellAddress;
    QHash<QString, QString> m_states;
    bool m_rebuildingParts = false;
};
