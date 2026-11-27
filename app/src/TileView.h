// SPDX-License-Identifier: MPL-2.0
// Khung hiển thị tài liệu: vẽ tile do engine render, tự vẽ con trỏ ô, vùng chọn,
// con trỏ văn bản và (với Excel) tiêu đề hàng/cột. Không dùng bất kỳ cửa sổ LibreOffice nào.
#pragma once
#include "dung/Engine.h"

#include <QAbstractScrollArea>
#include <QHash>
#include <QImage>
#include <QRegion>
#include <QSet>
#include <QTimer>
#include <QVector>

class QPainter;
class TileView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit TileView(dung::Document* doc, QWidget* parent = nullptr);

    int zoom() const { return m_zoom; }
    void setZoom(int percent);          // 25..400
    void resetForPartChange();          // gọi sau khi đổi sheet/slide/trang
    void handleEvent(int type, const QString& payload); // gọi trên luồng giao diện

    // ---- Dành cho kiểm thử/chẩn đoán ----
    bool hasCaret() const { return m_hasCaret; }
    bool renderPending() const { return !m_deferredPaint.isEmpty() || m_tilePaintTimer.isActive(); }
    int peakTilesPerPaint() const { return m_peakTilesPerPaint; }
    bool hasCellCursor() const { return m_hasCell; }
    QRect caretViewRect() const;        // con trỏ chữ trong toạ độ viewport (rỗng nếu chưa có)
    QString selectedCellRange(bool verifyGesture = false); // chỉ trả về địa chỉ khi khớp hình học vùng engine và header
    QRect cellCursorViewRect() const;   // khung ô hiện hành trong toạ độ viewport
    QRect cellSelectionViewRect() const; // visual outline only; not a write target
    bool caretShownNow() const;         // con trỏ chữ đang được vẽ ở khung hình này
    // Khoá pha nhấp nháy (bật/tắt) để so khung hình; unfreeze trả về nhấp nháy bình thường.
    void freezeBlink(bool on) { m_blinkFrozen = true; m_blinkOn = on; viewport()->update(); }
    void unfreezeBlink() { m_blinkFrozen = false; restartBlink(); }

signals:
    void zoomChanged(int percent);
    void cellSelectionFinished();

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
    void focusInEvent(QFocusEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
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
    QSet<quint64> m_dirtyTiles; // stale images are usable only on the same part/zoom
    QRegion m_deferredPaint;
    QTimer m_tilePaintTimer;
    int m_peakTilesPerPaint=0;
    QRect m_cellCursor, m_caret;   // twip; m_caret có thể rộng 0 nên dùng cờ m_hasCaret/m_hasCell, KHÔNG dùng isEmpty()
    QRect m_cellArea;              // vùng nhiều ô (CELL_SELECTION_AREA)
    bool m_hasCaret = false, m_hasCell = false;
    bool m_blinkFrozen = false;
    void updateCaretRegion();
    QRegion selectionPaintRegion() const;
    void restartBlink();           // hiện ngay rồi mới nhấp nháy lại (gõ/di chuyển/lấy focus)
    void drawCaret(QPainter& p);
    QRect cellOutlineTwips() const;
    void cancelComposition();       // huỷ chữ đang soạn (mất focus / Escape / bấm chuột)
    bool m_composing = false;       // engine đang giữ chữ đang soạn (preedit)
    bool m_caretVisible = false;
    bool m_caretVisSeen = false;   // engine đã từng gửi CURSOR_VISIBLE chưa
    bool m_blinkOn = true;
    QTimer m_blink;                // nhấp nháy con trỏ phía ứng dụng
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
    int m_clickCount = 1;
    QPoint m_pickStart, m_pickEnd;
    bool m_pickValid = false;
    bool m_pickComplete = false;
    bool m_fitted = false;
};
