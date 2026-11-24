// SPDX-License-Identifier: MPL-2.0
// Khung hiển thị tài liệu: vẽ tile do engine render, tự vẽ con trỏ ô, vùng chọn,
// con trỏ văn bản và (với Excel) tiêu đề hàng/cột. Không dùng bất kỳ cửa sổ LibreOffice nào.
#pragma once
#include "dung/Engine.h"

#include <QAbstractScrollArea>
#include <QHash>
#include <QImage>
#include <QTimer>
#include <QVector>

class TileView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit TileView(dung::Document* doc, QWidget* parent = nullptr);

    int zoom() const { return m_zoom; }
    void setZoom(int percent);          // 25..400
    void resetForPartChange();          // gọi sau khi đổi sheet/slide/trang
    void handleEvent(int type, const QString& payload); // gọi trên luồng giao diện

signals:
    void zoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void inputMethodEvent(QInputMethodEvent*) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
    bool focusNextPrevChild(bool) override { return false; } // Tab dành cho tài liệu

private:
    static constexpr int kTilePx = 256;
    double twipsPerPx() const { return 15.0 * 100.0 / m_zoom; }
    int tileTwips() const { return (int)(kTilePx * twipsPerPx()); }
    QPoint headerSize() const;                       // (rộng cột tiêu đề hàng, cao hàng tiêu đề cột)
    QPoint origin() const;                           // góc trên-trái của tài liệu trong viewport (khi chưa cuộn)
    void fitWidthOnce();
    QPoint toTwips(const QPoint& viewPos) const;     // toạ độ viewport -> twip tài liệu
    QRect toView(const QRect& twips) const;          // hình chữ nhật twip -> viewport
    void updateScrollBars();
    void sendVisibleArea();
    void requestHeaders();
    void invalidateTwips(const QRect& r);
    void sendMouse(int type, QMouseEvent* e, int count);
    static QRect parseRect(const QString& s, bool* ok = nullptr);
    static QVector<QRect> parseRects(const QString& s);

    dung::Document* m_doc;
    bool m_isCalc;
    int m_zoom = 100;
    long m_docW = 0, m_docH = 0;               // twip
    QHash<quint64, QImage> m_tiles;
    QRect m_cellCursor, m_caret;
    bool m_caretVisible = false;
    QVector<QRect> m_selection;
    // Tiêu đề hàng/cột (Excel): vị trí kết thúc tính theo pixel ở zoom hiện tại
    struct Header { QString text; int endPx; };
    QVector<Header> m_rows, m_cols;
    int m_rowStartPx = 0, m_colStartPx = 0;
    QTimer m_headerTimer;
    QTimer m_areaTimer;
    QTimer m_fitTimer;
    QTimer m_synthTimer;   // gộp các yêu cầu vẽ lại do tầng engine phát (SYNTH)
    QTimer m_synthLate;    // vẽ lại lần hai: lệnh UNO được engine xử lý trễ sau khi gửi
    void refreshAllTiles();
    int m_pressButtons = 0;
    bool m_fitted = false;
};
