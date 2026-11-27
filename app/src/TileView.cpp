// SPDX-License-Identifier: MPL-2.0
#define LOK_USE_UNSTABLE_API
#include "TileView.h"
#include "tools/dataassist/ExcelBridge.h"
#include <QGuiApplication>
#include <QInputMethod>

#include "CaretGeom.h"
#include "CaretTrace.h"
#include "KeyMap.h"
#include "Theme.h"

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <QJsonArray>
#include <QElapsedTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegion>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

namespace {
// Opt-in measurement only: never log keystrokes, cell addresses or text.
class UiTiming {
public:
    explicit UiTiming(const char* name):name_(name) {
        static const bool enabled=qEnvironmentVariableIntValue("DUNG_TRACE_INPUT_PERF")!=0;
        if(enabled) timer_.start();
    }
    ~UiTiming() {
        if(timer_.isValid()) {
            const auto ns=timer_.nsecsElapsed();
            if(ns>=16000000) qInfo("DUNG_INPUT_PERF %s %.2f ms",name_,double(ns)/1000000.0);
        }
    }
private:
    const char* name_;
    QElapsedTimer timer_;
};
}

TileView::TileView(dung::Document* doc, QWidget* parent)
    : QAbstractScrollArea(parent), m_doc(doc), m_isCalc(doc->kind() == dung::DocKind::Spreadsheet) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, !m_doc->readOnly());
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
    viewport()->setMouseTracking(false);
    setFrameShape(QFrame::NoFrame);
    horizontalScrollBar()->setSingleStep(40);
    verticalScrollBar()->setSingleStep(40);

    m_dragTimer.setSingleShot(true);
    m_dragTimer.setInterval(8);
    m_dragTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_dragTimer,&QTimer::timeout,this,&TileView::flushDragMove);
    m_tilePaintTimer.setSingleShot(true);
    m_tilePaintTimer.setInterval(8);
    connect(&m_tilePaintTimer,&QTimer::timeout,this,[this] {
        const auto region=m_deferredPaint;
        m_deferredPaint=QRegion();
        if(isVisible()&&!region.isEmpty())viewport()->update(region);
    });
    m_headerTimer.setSingleShot(true);
    m_headerTimer.setInterval(40);
    connect(&m_headerTimer, &QTimer::timeout, this, [this] {
        // Header queries run synchronously on the engine queue. Defer automatic
        // refresh while typing or dragging; explicit target verification still
        // calls requestHeaders() immediately.
        if(!isVisible())return;
        if(!m_doc->inputIdleFor(80)){m_headerTimer.start();return;}
        requestHeaders();
    });
    m_synthTimer.setSingleShot(true);
    m_synthTimer.setInterval(60);
    connect(&m_synthTimer, &QTimer::timeout, this, &TileView::refreshAllTiles);
    m_blink.setInterval(530);
    connect(&m_blink, &QTimer::timeout, this, [this] {
        if (m_blinkFrozen) return;
        m_blinkOn = !m_blinkOn;
        updateCaretRegion();
    });
    m_blink.start();
    m_synthLate.setSingleShot(true);
    m_synthLate.setInterval(250);
    connect(&m_synthLate, &QTimer::timeout, this, &TileView::refreshAllTiles);
    m_areaTimer.setSingleShot(true);
    m_areaTimer.setInterval(60);
    connect(&m_areaTimer, &QTimer::timeout, this, &TileView::sendVisibleArea);
    // Vừa khung chạy sau khi bố cục đã ổn định (tránh tính theo kích thước tạm lúc khởi tạo)
    m_fitTimer.setSingleShot(true);
    m_fitTimer.setInterval(200);
    connect(&m_fitTimer, &QTimer::timeout, this, &TileView::fitWidthOnce);

    m_doc->documentSize(m_docW, m_docH);
    updateScrollBars();
    sendVisibleArea();
}

QPoint TileView::headerSize() const { return m_isCalc ? QPoint(46, 22) : QPoint(0, 0); }

QPoint TileView::origin() const {
    if (m_isCalc) return headerSize();
    // Word/PowerPoint/PDF: căn giữa theo chiều ngang khi trang hẹp hơn khung nhìn, chừa lề 12px.
    const int docPx = (int)(m_docW / twipsPerPx());
    const int x = std::max(12, (viewport()->width() - docPx) / 2);
    return QPoint(x, 12);
}

void TileView::fitWidthOnce() {
    if (m_fitted || m_isCalc || m_docW <= 0 || m_docH <= 0 || !isVisible() || viewport()->width() < 400) return;
    m_fitted = true;
    const double fitW = (viewport()->width() - 48) * 15.0 * 100.0 / m_docW;
    const double fitH = (viewport()->height() - 36) * 15.0 * 100.0 / m_docH;
    if (m_doc->kind() == dung::DocKind::Presentation) setZoom(std::clamp((int)std::min(fitW, fitH), 25, 100)); // cả slide
    else if (m_doc->kind() == dung::DocKind::Drawing) setZoom(std::clamp((int)fitW, 25, 100));             // PDF: vừa chiều rộng
}

QPoint TileView::toTwips(const QPoint& p) const {
    const QPoint h = origin();
    const double tpp = twipsPerPx();
    return QPoint((int)std::lround((p.x() - h.x() + horizontalScrollBar()->value()) * tpp),
                  (int)std::lround((p.y() - h.y() + verticalScrollBar()->value()) * tpp));
}

QRect TileView::toView(const QRect& t) const {
    const QPoint h = origin();
    const double tpp = twipsPerPx();
    const int x = (int)std::floor(t.x() / tpp) - horizontalScrollBar()->value() + h.x();
    const int y = (int)std::floor(t.y() / tpp) - verticalScrollBar()->value() + h.y();
    return QRect(x, y, std::max(1, (int)std::ceil(t.width() / tpp)), std::max(1, (int)std::ceil(t.height() / tpp)));
}

void TileView::setZoom(int percent) {
    flushDragMove();
    percent = std::clamp(percent, 25, 400);
    if (percent == m_zoom) return;
    // Giữ điểm giữa màn hình khi đổi zoom
    const QPoint center = toTwips(viewport()->rect().center());
    m_zoom = percent;
    m_tiles.clear(); m_dirtyTiles.clear(); m_deferredPaint=QRegion(); m_tilePaintTimer.stop();
    updateScrollBars();
    const QPoint h = headerSize();
    horizontalScrollBar()->setValue((int)(center.x() / twipsPerPx()) - (viewport()->width() - h.x()) / 2);
    verticalScrollBar()->setValue((int)(center.y() / twipsPerPx()) - (viewport()->height() - h.y()) / 2);
    sendVisibleArea();
    viewport()->update();
    emit zoomChanged(m_zoom);
}

void TileView::resetForPartChange() {
    clearDragMove();
    m_hasCell = false; m_cellCursor = QRect(); m_rows.clear(); m_cols.clear(); m_pickValid = false; m_pickComplete = false;
    m_tiles.clear(); m_dirtyTiles.clear(); m_deferredPaint=QRegion(); m_tilePaintTimer.stop();
    m_selection.clear();
    m_cellArea = QRect();
    m_hasCaret = false;
    m_caret = QRect();
    m_caretVisible = false;
    m_caretVisSeen = false;
    m_doc->documentSize(m_docW, m_docH);
    horizontalScrollBar()->setValue(0);
    verticalScrollBar()->setValue(0);
    updateScrollBars();
    sendVisibleArea();
    viewport()->update();
}

void TileView::updateScrollBars() {
    const QPoint h = headerSize();
    double w = m_docW / twipsPerPx(), hh = m_docH / twipsPerPx();
    if (m_isCalc) { w += viewport()->width(); hh += viewport()->height(); } // bảng tính: cho cuộn thêm để mở rộng
    else { w += 24; hh += 24; }
    horizontalScrollBar()->setRange(0, std::max(0, (int)w - viewport()->width() + h.x()));
    verticalScrollBar()->setRange(0, std::max(0, (int)hh - viewport()->height() + h.y()));
    horizontalScrollBar()->setPageStep(viewport()->width());
    verticalScrollBar()->setPageStep(viewport()->height());
}

void TileView::sendVisibleArea() {
    if(!isVisible())return;
    const QPoint h = headerSize();
    const double tpp = twipsPerPx();
    const int x = (int)(horizontalScrollBar()->value() * tpp), y = (int)(verticalScrollBar()->value() * tpp);
    const int w = (int)((viewport()->width() - h.x()) * tpp), hh = (int)((viewport()->height() - h.y()) * tpp);
    m_doc->setClientView(kTilePx, kTilePx, tileTwips(), tileTwips(), x, y, std::max(w, 1), std::max(hh, 1));
    if (m_isCalc) m_headerTimer.start();
}

void TileView::requestHeaders() {
    const UiTiming timing("headers");
    if (!m_isCalc) return;
    const QPoint h = headerSize();
    const double tpp = twipsPerPx();
    const int x = (int)(horizontalScrollBar()->value() * tpp), y = (int)(verticalScrollBar()->value() * tpp);
    const int w = (int)((viewport()->width() - h.x()) * tpp) + 1, hh = (int)((viewport()->height() - h.y()) * tpp) + 1;
    const std::string q = ".uno:ViewRowColumnHeaders?x=" + std::to_string(x) + "&y=" + std::to_string(y) +
                          "&width=" + std::to_string(w) + "&height=" + std::to_string(hh);
    const QJsonObject o = QJsonDocument::fromJson(QByteArray::fromStdString(m_doc->commandValues(q))).object();
    auto parse = [](const QJsonArray& a, QVector<Header>& out, int& startPx) {
        out.clear();
        startPx = 0;
        for (int i = 0; i < a.size(); ++i) {
            const QJsonObject e = a.at(i).toObject();
            const QJsonValue sz = e.value(QStringLiteral("size"));
            const int px = sz.isString() ? sz.toString().toInt() : sz.toInt();
            const QJsonValue tx = e.value(QStringLiteral("text"));
            if (i == 0) { startPx = px; continue; }   // phần tử đầu: vị trí bắt đầu
            out.push_back({tx.isString() ? tx.toString() : QString::number(tx.toInt()), px});
        }
    };
    parse(o.value(QStringLiteral("rows")).toArray(), m_rows, m_rowStartPx);
    parse(o.value(QStringLiteral("columns")).toArray(), m_cols, m_colStartPx);
    viewport()->update();
}

int TileView::tileCacheLimit() const {
    // Allow a complete visible viewport plus a small scroll margin, including 4K.
    const qint64 cols=(qint64(viewport()->width())+kTilePx-1)/kTilePx+2;
    const qint64 rows=(qint64(viewport()->height())+kTilePx-1)/kTilePx+2;
    return int(qBound<qint64>(64,cols*rows+24,600));
}

void TileView::trimTileCache(int limit) {
    const qint64 cx=(qint64(horizontalScrollBar()->value())+viewport()->width()/2)/kTilePx;
    const qint64 cy=(qint64(verticalScrollBar()->value())+viewport()->height()/2)/kTilePx;
    while(m_tiles.size()>limit) {
        auto victim=m_tiles.begin();
        qint64 farthest=-1;
        for(auto it=m_tiles.begin();it!=m_tiles.end();++it) {
            const qint64 dx=qint64(it.key()>>32)-cx;
            const qint64 dy=qint64(it.key()&0xffffffff)-cy;
            const qint64 distance=qAbs(dx)+qAbs(dy);
            if(distance>farthest){farthest=distance;victim=it;}
        }
        m_dirtyTiles.remove(victim.key());
        m_tiles.erase(victim);
    }
}

void TileView::hideEvent(QHideEvent* event) {
    m_tilePaintTimer.stop();
    m_deferredPaint=QRegion();
    m_headerTimer.stop();
    m_areaTimer.stop();
    if(m_synthTimer.isActive()||m_synthLate.isActive()) {
        // A pending synthetic refresh must not be lost while hidden.
        for(auto it=m_tiles.cbegin();it!=m_tiles.cend();++it)m_dirtyTiles.insert(it.key());
    }
    m_synthTimer.stop();
    m_synthLate.stop();
    m_blink.stop();
    // Retain nearby tiles for quick tab switching, not an entire scroll history.
    trimTileCache(16);
    QAbstractScrollArea::hideEvent(event);
}

void TileView::showEvent(QShowEvent* event) {
    QAbstractScrollArea::showEvent(event);
    // Set engine zoom/visible area before the first paint of a reopened tab.
    m_areaTimer.stop();
    sendVisibleArea();
    m_blink.start();
    viewport()->update();
}

void TileView::paintEvent(QPaintEvent* ev) {
    const UiTiming timing("paint");
    QPainter p(viewport());
    const QPoint h = origin();
    const QRect content = viewport()->rect().adjusted(headerSize().x(), headerSize().y(), 0, 0);
    p.fillRect(viewport()->rect(), m_isCalc ? Qt::white : QColor(0xE9, 0xEE, 0xF5));
    if (!m_isCalc) {
        // Bóng đổ nhẹ quanh trang
        const QRect page(h.x() - horizontalScrollBar()->value(), h.y() - verticalScrollBar()->value(),
                         (int)(m_docW / twipsPerPx()), (int)(m_docH / twipsPerPx()));
        p.fillRect(page.translated(2, 2), QColor(0, 0, 0, 28));
    }

    // Render a small batch per paint event. A single engine tile may still
    // exceed the budget, but never synchronously render a whole cold viewport.
    QElapsedTimer frameBudget;
    frameBudget.start();
    int rendered=0;
    QRegion deferred;
    // ---- Tile ----
    p.save();
    p.setClipRect(content);
    const int sx = horizontalScrollBar()->value(), sy = verticalScrollBar()->value();
    const int tx0 = sx / kTilePx, ty0 = sy / kTilePx;
    const int tx1 = (sx + viewport()->width()) / kTilePx, ty1 = (sy + viewport()->height()) / kTilePx;
    const int maxTx = (int)(m_docW / twipsPerPx()) / kTilePx, maxTy = (int)(m_docH / twipsPerPx()) / kTilePx;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const QRect target(tx * kTilePx - sx + h.x(), ty * kTilePx - sy + h.y(), kTilePx, kTilePx);
            if (!ev->region().intersects(target)) continue;
            if (!m_isCalc && (tx > maxTx || ty > maxTy)) continue; // ngoài trang
            const quint64 key = (quint64(tx) << 32) | quint64(ty);
            auto it = m_tiles.find(key);
            if(it==m_tiles.end() || m_dirtyTiles.contains(key)) {
                if(rendered>0&&(rendered>=4||frameBudget.elapsed()>=12)) {
                    deferred |= QRegion(target.intersected(content));
                } else {
                    QImage img(kTilePx,kTilePx,QImage::Format_ARGB32_Premultiplied);
                    img.fill(Qt::white);
                    m_doc->paintTile(img.bits(),kTilePx,kTilePx,tx*tileTwips(),ty*tileTwips(),tileTwips(),tileTwips());
                    ++rendered;
                    if(it==m_tiles.end())trimTileCache(tileCacheLimit()-1);
                    it=m_tiles.insert(key,img);
                    m_dirtyTiles.remove(key);
                }
            }
            if(it!=m_tiles.end())p.drawImage(target.topLeft(),it.value());
            else p.fillRect(target,Qt::white);
        }
    }
    m_peakTilesPerPaint=qMax(m_peakTilesPerPaint,rendered);
    if(!deferred.isEmpty()) {
        m_deferredPaint |= deferred;
        if(!m_tilePaintTimer.isActive())m_tilePaintTimer.start();
    }
    // ---- Lớp phủ: vùng chọn, con trỏ ô, con trỏ văn bản ----
    const QRect area = cellOutlineTwips();
    const QRect outline = area.isEmpty() ? QRect() : toView(area);
    const bool multiCell = m_isCalc && !outline.isEmpty() && area != m_cellCursor;
    if (multiCell) {
        QColor tint = theme::kExcel; tint.setAlpha(32);
        QRegion shade(outline);
        if (m_hasCell) shade -= QRegion(toView(m_cellCursor));
        p.save(); p.setClipRegion(shade, Qt::IntersectClip);
        p.fillRect(outline, tint); p.restore();
    } else {
        // Text selections (including editing a Calc cell) retain the text-selection color.
        QColor sel = theme::kBlue; sel.setAlpha(50);
        for (const QRect& r : m_selection) p.fillRect(toView(r), sel);
    }
    if (!outline.isEmpty()) {
        p.setPen(QPen(theme::kExcel, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(outline.adjusted(0, 0, -1, -1));
        // Preserve the existing handle only for an explicitly identified cell/range.
        if (!m_cellArea.isEmpty() || area == m_cellCursor)
            p.fillRect(QRect(outline.right() - 3, outline.bottom() - 3, 6, 6), theme::kExcel);
    }
    drawCaret(p);
    p.restore();

    // ---- Tiêu đề hàng/cột (Excel) ----
    if (m_isCalc) {
        const QColor bg(0xF3, 0xF6, 0xFA), line(0xD9, 0xE1, 0xEC);
        p.fillRect(QRect(0, 0, viewport()->width(), h.y()), bg);
        p.fillRect(QRect(0, 0, h.x(), viewport()->height()), bg);
        p.setPen(line);
        p.drawLine(0, h.y() - 1, viewport()->width(), h.y() - 1);
        p.drawLine(h.x() - 1, 0, h.x() - 1, viewport()->height());
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.85);
        p.setFont(f);
        int prev = m_colStartPx;
        for (const Header& c : m_cols) {
            const QRect r(prev - sx + h.x(), 0, c.endPx - prev, h.y());
            p.setPen(line);
            p.drawLine(r.right(), 0, r.right(), h.y());
            p.setPen(theme::kMuted);
            if (r.right() > h.x()) p.drawText(r, Qt::AlignCenter, c.text);
            prev = c.endPx;
        }
        prev = m_rowStartPx;
        for (const Header& rw : m_rows) {
            const QRect r(0, prev - sy + h.y(), h.x(), rw.endPx - prev);
            p.setPen(line);
            p.drawLine(0, r.bottom(), h.x(), r.bottom());
            p.setPen(theme::kMuted);
            if (r.bottom() > h.y()) p.drawText(r, Qt::AlignCenter, rw.text);
            prev = rw.endPx;
        }
        p.fillRect(QRect(0, 0, h.x() - 1, h.y() - 1), bg);
    }
}


bool TileView::caretShownNow() const {
    return m_hasCaret && m_caretVisible && m_blinkOn && hasFocus();
}

QRect TileView::caretViewRect() const { return m_hasCaret ? toView(m_caret) : QRect(); }
QRect TileView::cellCursorViewRect() const { return m_hasCell ? toView(m_cellCursor) : QRect(); }

QRect TileView::cellOutlineTwips() const {
    if (!m_isCalc) return {};
    if (!m_cellArea.isEmpty()) return m_cellArea;
    if (!m_hasCell) return {};
    // Some engine versions report a selected rectangle through TEXT_SELECTION.
    // Use only a contiguous rectangle containing the active cell, and only for
    // painting. Do NOT promote this fallback to selectedCellRange()/write targets.
    if (!(m_hasCaret && m_caretVisible) && !m_selection.isEmpty()) {
        QRegion cells;
        for (const auto& r : m_selection) if (!r.isEmpty()) cells |= QRegion(r);
        const QRect bounds = cells.boundingRect();
        if (!bounds.isEmpty() && bounds.contains(m_cellCursor) && cells == QRegion(bounds))
            return bounds;
    }
    return m_cellCursor;
}

QRect TileView::cellSelectionViewRect() const {
    const QRect r=cellOutlineTwips();
    return r.isEmpty() ? QRect() : toView(r);
}

void TileView::drawCaret(QPainter& p) {
    if (carettrace::enabled()) {
        static bool last = false;
        const bool now = caretShownNow();
        if (now != last) {
            last = now;
            carettrace::log("vẽ: con trỏ %s (có=%d hiện=%d pha=%d focus=%d) viewRect=%d,%d,%d,%d", now ? "BẬT" : "tắt", int(m_hasCaret),
                            int(m_caretVisible), int(m_blinkOn), int(hasFocus()), caretViewRect().x(), caretViewRect().y(),
                            caretViewRect().width(), caretViewRect().height());
        }
    }
    if (!caretShownNow()) return;
    const QRectF c = caretgeom::caretDeviceRect(toView(m_caret), devicePixelRatioF());
    p.fillRect(c, QColor(0x1B, 0x2A, 0x41));
}

QRegion TileView::selectionPaintRegion() const {
    QRegion region;
    const QRect clip=viewport()->rect();
    const QRect outline=cellSelectionViewRect();
    if(!outline.isEmpty())region|=outline.adjusted(-5,-5,5,5).intersected(clip);
    for(const auto& rect:m_selection)
        region|=toView(rect).adjusted(-2,-2,2,2).intersected(clip);
    return region;
}

void TileView::updateCaretRegion() {
    if (m_hasCaret) viewport()->update(toView(m_caret).adjusted(-3,-3,3,3));
}

void TileView::restartBlink() {
    m_blinkOn = true;
    m_blink.start();   // khởi động lại chu kỳ: hiện liền ~530ms rồi mới tắt
    updateCaretRegion();
}

void TileView::focusInEvent(QFocusEvent* e) {
    QAbstractScrollArea::focusInEvent(e);
    carettrace::log("focus: VÀO");
    restartBlink();
    viewport()->update();
}

void TileView::resizeEvent(QResizeEvent* e) {
    QAbstractScrollArea::resizeEvent(e);
    updateScrollBars();
    if (!m_fitted) m_fitTimer.start();
    m_areaTimer.start();
}

void TileView::scrollContentsBy(int, int) {
    viewport()->update();
    m_areaTimer.start();
    if (m_isCalc) m_headerTimer.start();
}

QRect TileView::parseRect(const QString& s, bool* ok) {
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() < 4) { if (ok) *ok = false; return {}; }
    if (ok) *ok = true;
    return QRect(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt(),
                 parts[3].trimmed().toInt());
}

QVector<QRect> TileView::parseRects(const QString& s) {
    QVector<QRect> out;
    if (s.isEmpty() || s.startsWith(QLatin1String("EMPTY"))) return out;
    for (const QString& part : s.split(QLatin1Char(';'))) {
        bool ok = false;
        const QRect r = parseRect(part, &ok);
        if (ok && !r.isEmpty()) out.push_back(r);
    }
    return out;
}

void TileView::refreshAllTiles() {
    for(auto it=m_tiles.cbegin();it!=m_tiles.cend();++it)m_dirtyTiles.insert(it.key());
    if(isVisible()) {
        viewport()->update();
        if(m_isCalc)m_headerTimer.start();
    }
}

void TileView::invalidateTwips(const QRect& r) {
    const int tt = tileTwips();
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        const int tx = int(it.key() >> 32), ty = int(it.key() & 0xffffffff);
        const QRect tr(tx * tt, ty * tt, tt, tt);
        if(tr.intersects(r))m_dirtyTiles.insert(it.key());
        ++it;
    }
    viewport()->update(toView(r).intersected(viewport()->rect()));
}

void TileView::handleEvent(int type, const QString& payload) {
    if (carettrace::enabled()) {
        switch (type) {
        case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR: case LOK_CALLBACK_CURSOR_VISIBLE: case LOK_CALLBACK_CELL_CURSOR:
        case LOK_CALLBACK_TEXT_SELECTION: case LOK_CALLBACK_TEXT_SELECTION_START: case LOK_CALLBACK_TEXT_SELECTION_END:
        case LOK_CALLBACK_GRAPHIC_SELECTION: case LOK_CALLBACK_CELL_SELECTION_AREA: case LOK_CALLBACK_CELL_ADDRESS:
        case LOK_CALLBACK_INVALIDATE_VIEW_CURSOR: case LOK_CALLBACK_VIEW_CURSOR_VISIBLE:
            carettrace::log("sự kiện %d: %s", type, payload.left(120).toUtf8().constData());
            break;
        default: break;
        }
    }
    switch (type) {
    case LOK_CALLBACK_INVALIDATE_TILES: {
        // Engine Windows không tự báo làm mới sau khi gõ/ra lệnh: tầng engine phát "SYNTH".
        if (payload == QLatin1String("SYNTH")) {
            if(!isVisible()) {
                for(auto it=m_tiles.cbegin();it!=m_tiles.cend();++it)m_dirtyTiles.insert(it.key());
                break;
            }
            if (!m_synthTimer.isActive()) m_synthTimer.start();
            // Keep the leading refresh, but render the final fallback only after
            // the input burst ends, not every 250 ms while the user types.
            m_synthLate.start();
            break;
        }
        if (payload.startsWith(QLatin1String("EMPTY"))) { refreshAllTiles(); break; }
        bool ok = false;
        QRect r = parseRect(payload, &ok);
        if (ok) {
            // Calc có thể gửi chiều rộng/cao rất lớn (đến hết sheet) -> chặn tràn số.
            if (r.width() < 0 || r.width() > 100000000) r.setWidth(100000000);
            if (r.height() < 0 || r.height() > 100000000) r.setHeight(100000000);
            invalidateTwips(r);
        }
        if (m_isCalc&&isVisible()) m_headerTimer.start();
        break;
    }
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED: {
        const auto oldW=m_docW,oldH=m_docH;
        m_doc->documentSize(m_docW,m_docH);
        if(oldW!=m_docW||oldH!=m_docH) {
            updateScrollBars();
            if(!m_isCalc)refreshAllTiles();
        }
        break;
    }
    case LOK_CALLBACK_CELL_CURSOR:
        {
            const caretgeom::Parsed pc = caretgeom::parseCursorPayload(payload);
            m_hasCell = pc.valid;
            m_cellCursor = pc.valid ? pc.rect : QRect();
        }
        if (m_hasCell) {
            // Tự cuộn để ô đang chọn luôn nhìn thấy
            const QRect v = toView(m_cellCursor);
            const QPoint h = headerSize();
            if (v.right() > viewport()->width()) horizontalScrollBar()->setValue(horizontalScrollBar()->value() + v.right() - viewport()->width() + 20);
            else if (v.left() < h.x()) horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (h.x() - v.left()) - 20);
            if (v.bottom() > viewport()->height()) verticalScrollBar()->setValue(verticalScrollBar()->value() + v.bottom() - viewport()->height() + 20);
            else if (v.top() < h.y()) verticalScrollBar()->setValue(verticalScrollBar()->value() - (h.y() - v.top()) - 20);
        }
        viewport()->update();
        break;
    case LOK_CALLBACK_TEXT_SELECTION: {
        const auto next=parseRects(payload);
        if(next==m_selection)break;
        const auto old=selectionPaintRegion();
        m_selection=next;
        viewport()->update(old|selectionPaintRegion());
        break;
    }
    case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR: {
        updateCaretRegion(); // erase old caret even if the new one is elsewhere
        const caretgeom::Parsed pc = caretgeom::parseCursorPayload(payload);
        m_hasCaret = pc.valid;
        m_caret = pc.valid ? pc.rect : QRect();
        // Engine headless có thể không gửi CURSOR_VISIBLE: khi chưa từng thấy thì coi con trỏ là hiện.
        if (!m_caretVisSeen) m_caretVisible = m_hasCaret;
        restartBlink();   // đang gõ/di chuyển: hiện liên tục rồi nhấp nháy lại
        break;
    }
    case LOK_CALLBACK_CURSOR_VISIBLE:
        m_caretVisSeen = true;
        m_caretVisible = payload.contains(QLatin1String("true"));
        restartBlink();
        break;
    case LOK_CALLBACK_CELL_SELECTION_AREA: {
        // Vùng nhiều ô của Calc (nếu engine không báo qua TEXT_SELECTION): tô mờ giống vùng chọn chữ.
        const caretgeom::Parsed pa = caretgeom::parseCursorPayload(payload);
        const QRect next=(pa.valid && pa.rect.width()>0 && pa.rect.height()>0)?pa.rect:QRect();
        if(next==m_cellArea)break;
        const auto old=selectionPaintRegion();
        m_cellArea=next;
        viewport()->update(old|selectionPaintRegion());
        break;
    }
    default:
        break;
    }
}

void TileView::sendMouse(int type, QMouseEvent* e, int count) {
    const UiTiming timing("mouse");
    const QPoint h = headerSize();
    const QPoint pos = e->position().toPoint();
    if (m_isCalc && (pos.x() < h.x() || pos.y() < h.y())) return; // nhấp vào vùng tiêu đề (chưa hỗ trợ chọn cả hàng/cột)
    const QPoint t = toTwips(pos);
    int buttons = toLokButtons(e->buttons() | e->button());
    if (type == LOK_MOUSEEVENT_MOUSEBUTTONUP) buttons = m_pressButtons;
    m_doc->postMouse(type, t.x(), t.y(), count, buttons, toLokModifiers(e->modifiers()));
}

void TileView::mousePressEvent(QMouseEvent* e) {
    flushDragMove();
    if (m_isCalc) {
        const auto old=selectionPaintRegion();
        m_cellArea = QRect();
        m_selection.clear(); // do not paint the previous range while a new gesture starts
        viewport()->update(old|selectionPaintRegion());
        const QPoint pos = e->position().toPoint();
        const QPoint h = headerSize();
        m_pickComplete = false;
        m_pickValid = e->button() == Qt::LeftButton && pos.x() >= h.x() && pos.y() >= h.y();
        m_pickStart = toTwips(pos);
    }
    cancelComposition();
    setFocus();
    restartBlink();
    m_clickCount = 1;
    m_pressButtons = toLokButtons(e->buttons());
    sendMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, e, m_clickCount);
}
void TileView::mouseReleaseEvent(QMouseEvent* e) {
    flushDragMove(); // final queued position must reach engine before release
    sendMouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, e, m_clickCount);
    m_clickCount = 1;
    if (m_isCalc && e->button() == Qt::LeftButton && m_pickValid) {
        m_pickEnd = toTwips(e->position().toPoint());
        m_pickComplete = true;
        emit cellSelectionFinished();
    }
}
QString TileView::selectedCellRange(bool verifyGesture) {
    if (!m_isCalc || !m_hasCell) return {};
    const QRect area = !m_cellArea.isEmpty() ? m_cellArea : m_cellCursor;
    if (area.isEmpty()) return {};
    if (verifyGesture && (!m_pickComplete || !m_pickValid || !area.contains(m_pickStart) || !area.contains(m_pickEnd))) return {};
    requestHeaders(); // engine trả tên hàng/cột theo viewport hiện tại
    const double scale = twipsPerPx();
    auto edges = [](const QVector<Header>& headers, int start, double lo, double hi) -> QPair<QString, QString> {
        QString first, last;
        int prev = start;
        for (const auto& h : headers) {
            if (h.endPx > prev) {
                if (qAbs(double(prev) - lo) <= 2.0) first = h.text;
                if (qAbs(double(h.endPx) - hi) <= 2.0) last = h.text;
            }
            prev = h.endPx;
        }
        return {first, last};
    };
    const auto cols = edges(m_cols, m_colStartPx, area.x() / scale, (area.x() + area.width()) / scale);
    const auto rows = edges(m_rows, m_rowStartPx, area.y() / scale, (area.y() + area.height()) / scale);
    if (cols.first.isEmpty() || cols.second.isEmpty() || rows.first.isEmpty() || rows.second.isEmpty()) return {};
    const QString a = cols.first + rows.first, b = cols.second + rows.second;
    const auto range = dataassist::bridgeRangeAddress((a + QLatin1Char(':') + b).toStdString());
    return QString::fromStdString(range);
}
void TileView::clearDragMove() {
    m_dragTimer.stop();
    m_dragPending=false;
}

void TileView::flushDragMove() {
    if(!m_dragPending)return;
    const QPoint pos=m_dragTwips;
    const int buttons=m_dragButtons,modifiers=m_dragModifiers;
    clearDragMove();
    const UiTiming timing("drag");
    ++m_dragDispatches;
    m_doc->postMouse(LOK_MOUSEEVENT_MOUSEMOVE,pos.x(),pos.y(),1,buttons,modifiers);
}

void TileView::mouseMoveEvent(QMouseEvent* e) {
    if(e->buttons()==Qt::NoButton){flushDragMove();return;}
    // Preserve paths for drawing objects, slides, multi-button and modified drags.
    // Ordinary text/cell selection depends on the latest pointer endpoint.
    const bool simple=(m_isCalc||m_doc->kind()==dung::DocKind::Text)&&
        e->buttons()==Qt::LeftButton&&e->modifiers()==Qt::NoModifier;
    const QPoint pos=e->position().toPoint(),h=headerSize();
    if(!simple||(m_isCalc&&(pos.x()<h.x()||pos.y()<h.y()))) {
        flushDragMove();
        sendMouse(LOK_MOUSEEVENT_MOUSEMOVE,e,1);
        return;
    }
    m_dragTwips=toTwips(pos);
    m_dragButtons=toLokButtons(e->buttons());
    m_dragModifiers=toLokModifiers(e->modifiers());
    m_dragPending=true;
    if(!m_dragTimer.isActive())m_dragTimer.start();
}
void TileView::mouseDoubleClickEvent(QMouseEvent* e) {
    flushDragMove();
    setFocus();
    restartBlink();
    m_clickCount = 2;
    m_pressButtons = toLokButtons(e->buttons());
    sendMouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, e, m_clickCount);
}

void TileView::wheelEvent(QWheelEvent* e) {
    flushDragMove();
    if (e->modifiers() & Qt::ControlModifier) {
        setZoom(m_zoom + (e->angleDelta().y() > 0 ? 10 : -10));
        e->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(e);
}

void TileView::keyPressEvent(QKeyEvent* e) {
    flushDragMove();
    const UiTiming timing("key");
    if (m_composing && e->key() == Qt::Key_Escape) { cancelComposition(); e->accept(); return; }
    const LokKey k = toLokKey(e);
    if (!k.valid) { QAbstractScrollArea::keyPressEvent(e); return; }
    restartBlink();   // đang gõ: con trỏ hiện liên tục
    m_doc->postKey(LOK_KEYEVENT_KEYINPUT, k.charCode, k.keyCode);
    e->accept();
}

void TileView::keyReleaseEvent(QKeyEvent* e) {
    const LokKey k = toLokKey(e);
    if (!k.valid) return;
    m_doc->postKey(LOK_KEYEVENT_KEYUP, k.charCode, k.keyCode);
    e->accept();
}

void TileView::inputMethodEvent(QInputMethodEvent* e) {
    flushDragMove();
    const UiTiming timing("ime");
    if (m_doc->readOnly()) { e->ignore(); return; }
    restartBlink();
    // Chuẩn IME: preedit hiển thị ngay trong tài liệu (engine tự gạch chân), commit thay chữ đang soạn rồi chốt.
    // Logic quyết định nằm ở imePlan (hàm thuần, có test đơn vị).
    bool unsupported = false, after = false;
    const auto steps = imePlan(e->preeditString(), e->commitString(), e->replacementStart(), e->replacementLength(),
                               m_composing, &after, &unsupported);
    for (const ImeStep& st : steps) {
        switch (st.kind) {
        case ImeStep::Preedit:    m_doc->postExtTextInput(0, st.text.toUtf8().toStdString()); break;
        case ImeStep::EndPreedit: m_doc->postExtTextInput(2, std::string()); break;
        default: {
            const int keyCode = st.kind == ImeStep::Backspace ? 1283 : st.kind == ImeStep::Delete ? 1286 : 0;
            const int ch = st.kind == ImeStep::Char ? st.charCode : 0;
            m_doc->postKey(LOK_KEYEVENT_KEYINPUT, ch, keyCode);
            m_doc->postKey(LOK_KEYEVENT_KEYUP, ch, keyCode);
        }
        }
    }
    m_composing = after;
    e->accept();
}

void TileView::cancelComposition() {
    if (!m_composing) return;
    m_doc->postExtTextInput(0, std::string());   // xoá chữ đang soạn, trả về như trước khi soạn
    m_doc->postExtTextInput(2, std::string());
    m_composing = false;
    if (QGuiApplication::inputMethod()) QGuiApplication::inputMethod()->reset();
}

void TileView::focusOutEvent(QFocusEvent* e) {
    flushDragMove();
    carettrace::log("focus: RA");
    cancelComposition();
    QAbstractScrollArea::focusOutEvent(e);
    viewport()->update();
}

QVariant TileView::inputMethodQuery(Qt::InputMethodQuery q) const {
    if (q == Qt::ImCursorRectangle) return m_hasCaret ? toView(m_caret) : (m_hasCell ? toView(m_cellCursor) : QRect());
    if (q == Qt::ImEnabled) return !m_doc->readOnly();
    if (q == Qt::ImHints) return int(Qt::ImhNone);
    return QAbstractScrollArea::inputMethodQuery(q);
}
