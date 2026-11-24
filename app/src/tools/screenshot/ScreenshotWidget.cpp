// SPDX-License-Identifier: MPL-2.0
#include "ScreenshotWidget.h"
#include "ScreenshotCore.h"

#include <QAbstractButton>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

using namespace shot;

namespace dungshot {

// ---------- chụp ----------
struct ScreenShot { QScreen* screen; QPixmap pix; };

QVector<ScreenShot> grabScreens() {
    QVector<ScreenShot> v;
    for (QScreen* s : QGuiApplication::screens()) v.push_back({s, s->grabWindow(0)});
    return v;
}

// Ghép mọi màn hình thành một ảnh; DPI cao nhất làm chuẩn để không mất nét.
QImage composeAll(const QVector<ScreenShot>& shots) {
    if (shots.isEmpty()) return QImage();
    QRect all; double maxDpr = 1.0;
    for (const auto& s : shots) { all = all.united(s.screen->geometry()); maxDpr = qMax(maxDpr, s.screen->devicePixelRatio()); }
    QImage out(QSize(qRound(all.width() * maxDpr), qRound(all.height() * maxDpr)), QImage::Format_ARGB32);
    out.fill(Qt::black);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (const auto& s : shots) {
        const QRect g = s.screen->geometry();
        const QRect t(qRound((g.left() - all.left()) * maxDpr), qRound((g.top() - all.top()) * maxDpr),
                      qRound(g.width() * maxDpr), qRound(g.height() * maxDpr));
        p.drawPixmap(t, s.pix);
    }
    return out;
}

#ifdef Q_OS_WIN
// Danh sách cửa sổ nhìn thấy (từ trên xuống dưới theo thứ tự Z), toạ độ điểm ảnh thật.
BOOL CALLBACK enumWin(HWND h, LPARAM lp) {
    auto* v = reinterpret_cast<QVector<QRect>*>(lp);
    if (!IsWindowVisible(h) || IsIconic(h)) return TRUE;
    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return TRUE;
    RECT r;
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) GetWindowRect(h, &r);
    if (r.right - r.left < 24 || r.bottom - r.top < 24) return TRUE;
    v->push_back(QRect(r.left, r.top, r.right - r.left, r.bottom - r.top));
    return TRUE;
}
// Đổi hình chữ nhật điểm ảnh thật sang toạ độ logic của Qt (gốc màn hình = toạ độ thật, kích thước chia dpr).
QRect nativeToLogical(const QRect& n) {
    for (QScreen* s : QGuiApplication::screens()) {
        const qreal d = s->devicePixelRatio();
        const QRect nat(s->geometry().topLeft(), QSize(qRound(s->geometry().width() * d), qRound(s->geometry().height() * d)));
        if (nat.contains(n.center())) {
            const QPoint o = s->geometry().topLeft();
            return QRect(o + (n.topLeft() - o) / d, n.size() / d);
        }
    }
    return n;
}
QVector<QRect> visibleWindowsLogical() {
    QVector<QRect> nat, out;
    EnumWindows(enumWin, reinterpret_cast<LPARAM>(&nat));
    for (const QRect& r : nat) out.push_back(nativeToLogical(r));
    return out;
}
#else
QVector<QRect> visibleWindowsLogical() { return {}; }
#endif

// Lớp phủ chọn vùng/cửa sổ trên một màn hình, hiển thị ảnh đã "đóng băng".
class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    enum Mode { Region, Window };
    CaptureOverlay(const ScreenShot& s, Mode m, const QVector<QRect>& wins)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), m_shot(s), m_mode(m), m_wins(wins) {
        setScreen(s.screen);
        setGeometry(s.screen->geometry());
        setCursor(Qt::CrossCursor);
        setMouseTracking(true);
        setAttribute(Qt::WA_DeleteOnClose, false);
    }
signals:
    void captured(const QImage& img);
    void cancelled();
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawPixmap(rect(), m_shot.pix);
        const QRect sel = currentRect();
        QRegion dim(rect());
        if (!sel.isEmpty()) dim = dim.subtracted(QRegion(sel));
        p.setClipRegion(dim);
        p.fillRect(rect(), QColor(0, 0, 0, 110));
        p.setClipping(false);
        if (!sel.isEmpty()) {
            p.setPen(QPen(QColor(37, 99, 235), 2)); p.setBrush(Qt::NoBrush); p.drawRect(sel);
            const qreal d = m_shot.screen->devicePixelRatio();
            const QString t = QStringLiteral("%1 × %2 px").arg(qRound(sel.width() * d)).arg(qRound(sel.height() * d));
            p.setPen(Qt::white); p.setBrush(QColor(37, 99, 235));
            const QRect tb(sel.left(), qMax(0, sel.top() - 24), 120, 22);
            p.drawRect(tb); p.drawText(tb, Qt::AlignCenter, t);
        } else {
            p.setPen(Qt::white);
            p.drawText(QRect(0, 20, width(), 24), Qt::AlignHCenter,
                       m_mode == Region ? tr("Kéo chuột để chọn vùng — Esc để huỷ") : tr("Đưa chuột vào cửa sổ rồi bấm — Esc để huỷ"));
        }
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::RightButton) { emit cancelled(); return; }
        if (e->button() != Qt::LeftButton) return;
        m_press = e->position().toPoint(); m_cur = m_press; m_drag = true; update();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        m_cur = e->position().toPoint();
        if (m_mode == Window && !m_drag) {
            m_hover = QRect();
            const QPoint g = e->globalPosition().toPoint();
            for (const QRect& w : m_wins) if (w.contains(g)) { m_hover = w.translated(-geometry().topLeft()).intersected(rect()); break; }
        }
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton || !m_drag) return;
        m_drag = false; m_cur = e->position().toPoint();
        QRect sel = currentRect();
        if (m_mode == Window && (m_press - m_cur).manhattanLength() < 6) sel = m_hover;   // bấm không kéo = chọn cửa sổ đang chỉ
        if (sel.width() < 3 || sel.height() < 3) { if (m_mode == Window) return; m_press = m_cur; update(); return; }
        const qreal d = m_shot.screen->devicePixelRatio();
        const QRect px(qRound(sel.x() * d), qRound(sel.y() * d), qRound(sel.width() * d), qRound(sel.height() * d));
        emit captured(m_shot.pix.copy(px).toImage());
    }
    void keyPressEvent(QKeyEvent* e) override { if (e->key() == Qt::Key_Escape) emit cancelled(); }
private:
    QRect currentRect() const {
        if (m_mode == Window && !m_drag) return m_hover;
        if (!m_drag && m_press == m_cur) return QRect();
        return QRect(m_press, m_cur).normalized();
    }
    ScreenShot m_shot; Mode m_mode; QVector<QRect> m_wins;
    QPoint m_press, m_cur; bool m_drag = false; QRect m_hover;
};

// ---------- khung vẽ ----------
enum Tool { TRect, TArrow, TPen, THighlight, TText, TStep, TBlur, TMosaic, TCrop, TRuler };

class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget* parent) : QWidget(parent) { setMouseTracking(true); setMinimumSize(480, 300); setObjectName("canvas"); }
    void setDoc(Document* d) { m_doc = d; m_press = false; update(); emit changed(); }
    void setTool(int t) { m_tool = Tool(t); }
    void setColor(const QColor& c) { m_color = c; }
    void setWidthPx(int w) { m_width = w; }
    QImage rendered() const { return m_doc ? m_doc->render() : QImage(); }
signals:
    void changed();
    void info(const QString& s);
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(236, 241, 249));
        if (!m_doc) {
            p.setPen(QColor(100, 116, 139));
            p.drawText(rect(), Qt::AlignCenter, tr("Chưa có ảnh. Bấm \"Vùng\", \"Cửa sổ\" hoặc \"Toàn màn hình\" để chụp."));
            return;
        }
        QImage img = m_doc->render();
        if (m_press && m_tool != TCrop && m_tool != TRuler) {
            Document tmp(*m_doc);
            Op o; if (makeOp(o) && tmp.add(o)) img = tmp.render();
        }
        layoutFor(img.size());
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(m_target, img);
        p.setPen(QColor(203, 213, 225)); p.drawRect(m_target.adjusted(-1, -1, 0, 0));
        if (m_press && m_tool == TCrop) {
            const QRectF r = QRectF(toWidget(m_start), toWidget(m_cur)).normalized();
            p.setBrush(QColor(0, 0, 0, 90)); p.setPen(Qt::NoPen);
            QRegion dim(m_target.toRect()); dim = dim.subtracted(QRegion(r.toRect())); p.setClipRegion(dim); p.drawRect(m_target); p.setClipping(false);
            p.setBrush(Qt::NoBrush); p.setPen(QPen(Qt::white, 1, Qt::DashLine)); p.drawRect(r);
        }
        if (m_press && m_tool == TRuler) {   // thước chỉ vẽ trên màn hình, không đưa vào ảnh
            p.setPen(QPen(QColor(234, 88, 12), 2)); p.drawLine(toWidget(m_start), toWidget(m_cur));
            const Measure m = measure(m_start, m_cur);
            p.drawText(toWidget(m_cur) + QPointF(8, -8), QStringLiteral("%1 px (%2, %3)").arg(m.length, 0, 'f', 1).arg(qRound(m.dx)).arg(qRound(m.dy)));
        }
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (!m_doc || e->button() != Qt::LeftButton) return;
        const QPointF ip = toImage(e->position());
        if (m_tool == TText) {
            bool ok = false;
            const QString t = QInputDialog::getMultiLineText(this, tr("Chèn chữ"), tr("Nhập nội dung (có dấu tiếng Việt):"), QString(), &ok);
            if (ok && !t.trimmed().isEmpty()) { Op o; o.type = OpType::Text; o.a = ip; o.text = t; o.color = m_color; o.fontPx = qMax(12, m_width * 6); m_doc->add(o); update(); emit changed(); }
            return;
        }
        if (m_tool == TStep) {
            Op o; o.type = OpType::Step; o.a = ip; o.color = m_color; o.width = qMax(4, m_width); o.number = m_doc->nextStepNumber();
            m_doc->add(o); update(); emit changed(); return;
        }
        m_press = true; m_start = ip; m_cur = ip; m_path.clear(); m_path << ip; update();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_doc) return;
        const QPointF ip = toImage(e->position());
        emit info(QStringLiteral("x=%1, y=%2 px").arg(qRound(ip.x())).arg(qRound(ip.y())));
        if (!m_press) return;
        m_cur = ip; if (m_tool == TPen) m_path << ip;
        if (m_tool == TRuler) { const Measure m = measure(m_start, m_cur); emit info(tr("Thước: %1 px (ngang %2, dọc %3)").arg(m.length, 0, 'f', 1).arg(qRound(m.dx)).arg(qRound(m.dy))); }
        else if (m_tool == TCrop || m_tool == TBlur || m_tool == TMosaic) emit info(tr("Vùng: %1 × %2 px").arg(qRound(qAbs(m_cur.x() - m_start.x()))).arg(qRound(qAbs(m_cur.y() - m_start.y()))));
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (!m_doc || !m_press || e->button() != Qt::LeftButton) return;
        m_press = false; m_cur = toImage(e->position());
        if (m_tool != TRuler) {
            Op o;
            if (makeOp(o)) { if (!m_doc->add(o)) emit info(tr("Vùng quá nhỏ hoặc nằm ngoài ảnh, không áp dụng.")); }
        }
        update(); emit changed();
    }
private:
    bool makeOp(Op& o) const {
        o.color = m_color; o.width = m_width;
        switch (m_tool) {
        case TRect: o.type = OpType::Rect; o.a = m_start; o.b = m_cur; return true;
        case TArrow: o.type = OpType::Arrow; o.a = m_start; o.b = m_cur; return (m_cur - m_start).manhattanLength() > 3;
        case TPen: o.type = OpType::Pen; o.path = m_path; return true;
        case THighlight: o.type = OpType::Highlight; o.color = QColor(255, 235, 0); o.a = m_start; o.b = m_cur; return true;
        case TBlur: case TMosaic: case TCrop: {
            o.type = m_tool == TBlur ? OpType::Blur : m_tool == TMosaic ? OpType::Mosaic : OpType::Crop;
            o.region = QRectF(m_start, m_cur).normalized().toAlignedRect();
            if (o.type != OpType::Crop) o.width = qMax(kMinRedactBlock, m_width * 3);
            return true; }
        default: return false;
        }
    }
    QPointF cropOrigin() const { return m_doc ? QPointF(m_doc->currentCrop().topLeft()) : QPointF(); }
    void layoutFor(const QSize& img) const {
        const double s = qMin(1.0, qMin((width() - 16.0) / img.width(), (height() - 16.0) / img.height()));
        m_scale = qMax(0.05, s);
        const QSizeF sz(img.width() * m_scale, img.height() * m_scale);
        m_target = QRectF((width() - sz.width()) / 2, (height() - sz.height()) / 2, sz.width(), sz.height());
    }
    QPointF toImage(const QPointF& w) const {
        if (m_scale <= 0) return w;
        QPointF d((w.x() - m_target.x()) / m_scale, (w.y() - m_target.y()) / m_scale);
        return d + cropOrigin();
    }
    QPointF toWidget(const QPointF& i) const { const QPointF d = i - cropOrigin(); return QPointF(m_target.x() + d.x() * m_scale, m_target.y() + d.y() * m_scale); }

    Document* m_doc = nullptr;
    Tool m_tool = TRect; QColor m_color = QColor(220, 38, 38); int m_width = 4;
    bool m_press = false; QPointF m_start, m_cur; QVector<QPointF> m_path;
    mutable double m_scale = 1.0; mutable QRectF m_target;
};

#ifdef Q_OS_WIN
constexpr int kHotkeyId = 0x4455;   // "DU"
class HotkeyFilter : public QAbstractNativeEventFilter {
public:
    std::function<void()> callback;
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override {
        if (type == "windows_generic_MSG") {
            MSG* m = static_cast<MSG*>(message);
            if (m->message == WM_HOTKEY && int(m->wParam) == kHotkeyId) { if (callback) callback(); if (result) *result = 0; return true; }
        }
        return false;
    }
};
#endif

// ---------- widget chính ----------
class ScreenshotWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotWidget(QWidget* parent) : QWidget(parent) {
        setObjectName("screenshotWidget");
        auto* lay = new QVBoxLayout(this);
        // hàng chụp
        auto* cap = new QHBoxLayout;
        auto mk = [&](const QString& text, const char* name) { auto* b = new QPushButton(text, this); b->setObjectName(name); cap->addWidget(b); return b; };
        auto* bRegion = mk(tr("Vùng"), "capRegion");
        m_bWindow = mk(tr("Cửa sổ"), "capWindow");
        auto* bFull = mk(tr("Toàn màn hình"), "capFull");
        cap->addWidget(new QLabel(tr("Hẹn giờ (giây):"), this));
        m_delay = new QSpinBox(this); m_delay->setRange(0, 30); cap->addWidget(m_delay);
        m_hotkey = new QCheckBox(tr("Phím tắt toàn cục"), this);
        m_hotkeyKind = new QComboBox(this);
        m_hotkeyKind->addItems({QStringLiteral("Ctrl+Alt+S"), QStringLiteral("Ctrl+Shift+X"), QStringLiteral("Ctrl+Alt+PrtSc")});
        cap->addWidget(m_hotkey); cap->addWidget(m_hotkeyKind); cap->addStretch();
        lay->addLayout(cap);
        // hàng công cụ
        auto* tools = new QHBoxLayout;
        m_group = new QButtonGroup(this); m_group->setExclusive(true);
        struct T { Tool t; const char* label; const char* name; } list[] = {
            {TRect, "Khung", "tool_rect"}, {TArrow, "Mũi tên", "tool_arrow"}, {TPen, "Bút", "tool_pen"}, {THighlight, "Tô sáng", "tool_highlight"},
            {TText, "Chữ", "tool_text"}, {TStep, "Số bước", "tool_step"}, {TBlur, "Làm mờ", "tool_blur"}, {TMosaic, "Mosaic", "tool_mosaic"},
            {TCrop, "Cắt", "tool_crop"}, {TRuler, "Thước px", "tool_ruler"}};
        for (const auto& t : list) {
            auto* b = new QToolButton(this); b->setText(QString::fromUtf8(t.label)); b->setCheckable(true); b->setObjectName(t.name);
            b->setToolButtonStyle(Qt::ToolButtonTextOnly); m_group->addButton(b, t.t); tools->addWidget(b);
            if (t.t == TRect) b->setChecked(true);
        }
        m_colorBtn = new QPushButton(tr("Màu"), this); m_colorBtn->setObjectName("colorBtn"); setBtnColor(QColor(220, 38, 38));
        m_width = new QSpinBox(this); m_width->setRange(1, 24); m_width->setValue(4); m_width->setPrefix(tr("Nét ")); m_width->setObjectName("widthSpin");
        tools->addWidget(m_colorBtn); tools->addWidget(m_width); tools->addStretch();
        lay->addLayout(tools);
        // khung vẽ
        m_canvas = new Canvas(this); lay->addWidget(m_canvas, 1);
        // hàng lưu
        auto* act = new QHBoxLayout;
        m_undo = new QPushButton(tr("Hoàn tác"), this); m_undo->setObjectName("undoBtn");
        m_redo = new QPushButton(tr("Làm lại"), this); m_redo->setObjectName("redoBtn");
        m_copy = new QPushButton(tr("Sao chép"), this);
        m_fmt = new QComboBox(this); m_fmt->addItems({QStringLiteral("PNG"), QStringLiteral("JPG"), QStringLiteral("PDF")});
        m_save = new QPushButton(tr("Lưu vào thư mục mặc định"), this); m_save->setObjectName("saveBtn");
        m_saveAs = new QPushButton(tr("Lưu thành…"), this);
        auto* openDir = new QPushButton(tr("Mở thư mục"), this);
        m_insert = new QPushButton(tr("Chèn vào tài liệu đang mở"), this); m_insert->setObjectName("insertBtn");
        m_insert->setVisible(false);   // chỉ hiện khi nơi nhúng đăng ký hàm chèn (setInsertHandler)
        for (QWidget* w : {static_cast<QWidget*>(m_undo), static_cast<QWidget*>(m_redo), static_cast<QWidget*>(m_copy), static_cast<QWidget*>(m_fmt),
                           static_cast<QWidget*>(m_save), static_cast<QWidget*>(m_saveAs), static_cast<QWidget*>(openDir), static_cast<QWidget*>(m_insert)}) act->addWidget(w);
        act->addStretch();
        lay->addLayout(act);
        m_status = new QLabel(tr("Sẵn sàng. Ảnh chỉ được lưu khi bạn bấm Lưu; không bao giờ ghi đè tệp có sẵn."), this);
        m_status->setWordWrap(true); m_status->setObjectName("statusLbl");
        lay->addWidget(m_status);

#ifndef Q_OS_WIN
        m_bWindow->setEnabled(false); m_bWindow->setToolTip(tr("Chụp theo cửa sổ hiện chỉ có trên Windows."));
        m_hotkey->setVisible(false); m_hotkeyKind->setVisible(false);
#endif
        connect(bRegion, &QPushButton::clicked, this, [this] { startCapture(CaptureOverlay::Region, false); });
        connect(m_bWindow, &QPushButton::clicked, this, [this] { startCapture(CaptureOverlay::Window, false); });
        connect(bFull, &QPushButton::clicked, this, [this] { startCapture(CaptureOverlay::Region, true); });
        connect(m_group, &QButtonGroup::idClicked, this, [this](int id) { m_canvas->setTool(id); });
        connect(m_colorBtn, &QPushButton::clicked, this, [this] {
            const QColor c = QColorDialog::getColor(m_color, this, tr("Chọn màu"));
            if (c.isValid()) { setBtnColor(c); }
        });
        connect(m_width, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { m_canvas->setWidthPx(v); });
        connect(m_undo, &QPushButton::clicked, this, [this] { if (m_doc && m_doc->undo()) { m_canvas->update(); refresh(); } });
        connect(m_redo, &QPushButton::clicked, this, [this] { if (m_doc && m_doc->redo()) { m_canvas->update(); refresh(); } });
        connect(m_copy, &QPushButton::clicked, this, [this] { copyToClipboard(); });
        connect(m_insert, &QPushButton::clicked, this, [this] { insertIntoDocument(); });
        connect(m_save, &QPushButton::clicked, this, [this] { saveDefault(); });
        connect(m_saveAs, &QPushButton::clicked, this, [this] { saveAs(); });
        connect(openDir, &QPushButton::clicked, this, [] {
            const QString d = defaultSaveDir(); QDir().mkpath(d); QDesktopServices::openUrl(QUrl::fromLocalFile(d)); });
        connect(m_canvas, &Canvas::changed, this, [this] { refresh(); });
        connect(m_canvas, &Canvas::info, this, [this](const QString& s) { m_status->setText(s); });
        auto* su = new QShortcut(QKeySequence::Undo, this); su->setContext(Qt::WidgetWithChildrenShortcut);
        connect(su, &QShortcut::activated, this, [this] { m_undo->click(); });
        auto* sr = new QShortcut(QKeySequence::Redo, this); sr->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sr, &QShortcut::activated, this, [this] { m_redo->click(); });

        // phím tắt toàn cục: mặc định TẮT; chỉ bật khi người dùng tích chọn
        QSettings st("DungOffice", "Screenshot");
        m_hotkeyKind->setCurrentIndex(qBound(0, st.value("hotkeyKind", 0).toInt(), 2));
        connect(m_hotkey, &QCheckBox::toggled, this, [this](bool on) { setHotkey(on); });
        connect(m_hotkeyKind, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { if (m_hotkey->isChecked()) setHotkey(true); });
        if (st.value("hotkeyEnabled", false).toBool()) m_hotkey->setChecked(true);   // dùng lại lựa chọn của người dùng
        refresh();
    }
    ~ScreenshotWidget() override { setHotkey(false, false); }

    void loadImage(const QImage& img) {
        if (img.isNull()) return;
        m_doc = std::make_unique<Document>(img);
        m_canvas->setDoc(m_doc.get());
        m_status->setText(tr("Đã có ảnh %1 × %2 px. Chọn công cụ để chú thích, che thông tin hoặc cắt.").arg(img.width()).arg(img.height()));
        refresh();
    }
    QImage render() const { return m_doc ? m_doc->render() : QImage(); }
    // Nơi nhúng đăng ký hàm chèn ảnh vào tài liệu đang mở; trả về chuỗi lỗi (rỗng = thành công).
    void setInsertHandler(std::function<QString(const QImage&)> h) { m_insertFn = std::move(h); m_insert->setVisible(bool(m_insertFn)); }

private:
    void setBtnColor(const QColor& c) {
        m_color = c; m_canvas ? m_canvas->setColor(c) : void();
        m_colorBtn->setStyleSheet(QStringLiteral("QPushButton{border-left:14px solid %1;}").arg(c.name()));
    }
    void refresh() {
        m_undo->setEnabled(m_doc && m_doc->canUndo()); m_redo->setEnabled(m_doc && m_doc->canRedo());
        const bool has = bool(m_doc);
        m_copy->setEnabled(has); m_save->setEnabled(has); m_saveAs->setEnabled(has);
    }
    void restoreHost() { QWidget* top = window(); top->show(); top->raise(); top->activateWindow(); }
    void closeOverlays() { for (auto* o : m_overlays) { o->hide(); o->deleteLater(); } m_overlays.clear(); }

    void startCapture(CaptureOverlay::Mode mode, bool full) {
        QWidget* top = window();
        m_hadHost = top->isVisible();
        const int ms = 250 + m_delay->value() * 1000;     // 250 ms để cửa sổ này kịp ẩn khỏi ảnh chụp
        if (m_delay->value() > 0) m_status->setText(tr("Sẽ chụp sau %1 giây…").arg(m_delay->value()));
        top->hide();
        QTimer::singleShot(ms, this, [this, mode, full] { doCapture(mode, full); });
    }
    void doCapture(CaptureOverlay::Mode mode, bool full) {
        const QVector<ScreenShot> shots = grabScreens();
        if (shots.isEmpty()) { restoreHost(); m_status->setText(tr("Không chụp được màn hình.")); return; }
        if (full) { QImage img = composeAll(shots); restoreHost(); loadImage(img); return; }
        const QVector<QRect> wins = mode == CaptureOverlay::Window ? visibleWindowsLogical() : QVector<QRect>();
        for (const ScreenShot& s : shots) {
            auto* o = new CaptureOverlay(s, mode, wins);
            connect(o, &CaptureOverlay::captured, this, [this](const QImage& img) { closeOverlays(); restoreHost(); loadImage(img); });
            connect(o, &CaptureOverlay::cancelled, this, [this] { closeOverlays(); restoreHost(); m_status->setText(tr("Đã huỷ chụp.")); });
            m_overlays.push_back(o); o->show();
        }
        if (!m_overlays.isEmpty()) { m_overlays.first()->activateWindow(); m_overlays.first()->setFocus(); }
    }
    void copyToClipboard() {
        const QImage img = render(); if (img.isNull()) return;
        QGuiApplication::clipboard()->setImage(img);
        m_status->setText(tr("Đã sao chép ảnh vào clipboard."));
    }
    void insertIntoDocument() {
        const QImage img = render();
        if (img.isNull()) { m_status->setText(tr("Chưa có ảnh để chèn.")); return; }
        if (!m_insertFn) return;
        const QString err = m_insertFn(img);
        m_status->setText(err.isEmpty() ? tr("Đã chèn ảnh vào tài liệu đang mở.") : tr("Không chèn được: %1").arg(err));
    }
    Format curFormat() const { return m_fmt->currentIndex() == 1 ? Format::Jpg : m_fmt->currentIndex() == 2 ? Format::Pdf : Format::Png; }
    void saveDefault() {
        const QImage img = render(); if (img.isNull()) return;
        const SaveResult r = saveImage(img, defaultSaveDir(), curFormat(), QDateTime::currentDateTime());
        m_status->setText(r.ok ? tr("Đã lưu: %1").arg(QDir::toNativeSeparators(r.path)) : tr("Không lưu được: %1").arg(r.message));
    }
    void saveAs() {
        const QImage img = render(); if (img.isNull()) return;
        QDir().mkpath(defaultSaveDir());
        const QString start = QDir(defaultSaveDir()).filePath(timestampStem(QDateTime::currentDateTime()) + "." + extensionFor(curFormat()));
        const QString p = QFileDialog::getSaveFileName(this, tr("Lưu ảnh thành"), start, tr("Ảnh (*.png *.jpg *.pdf)"));
        if (p.isEmpty()) return;
        const SaveResult r = saveImageAs(img, p);
        m_status->setText(r.ok ? tr("%1 (%2)").arg(r.message, QDir::toNativeSeparators(r.path)) : tr("Không lưu được: %1").arg(r.message));
    }
    void setHotkey(bool on, bool persist = true) {
#ifdef Q_OS_WIN
        if (m_hkRegistered) { UnregisterHotKey(nullptr, kHotkeyId); m_hkRegistered = false; }
        if (on) {
            const int k = m_hotkeyKind->currentIndex();
            const UINT mod = (k == 1 ? (MOD_CONTROL | MOD_SHIFT) : (MOD_CONTROL | MOD_ALT)) | MOD_NOREPEAT;
            const UINT vk = k == 0 ? 'S' : k == 1 ? 'X' : VK_SNAPSHOT;
            if (RegisterHotKey(nullptr, kHotkeyId, mod, vk)) {
                m_hkRegistered = true;
                if (!m_filter) {
                    m_filter = std::make_unique<HotkeyFilter>();
                    m_filter->callback = [this] { QTimer::singleShot(0, this, [this] { startCapture(CaptureOverlay::Region, false); }); };
                    QCoreApplication::instance()->installNativeEventFilter(m_filter.get());
                }
                m_status->setText(tr("Đã bật phím tắt %1 (chụp vùng). Chỉ hoạt động khi Dũng Office đang chạy.").arg(m_hotkeyKind->currentText()));
            } else {
                m_status->setText(tr("Không đăng ký được phím tắt %1 (có thể ứng dụng khác đang dùng). Hãy chọn tổ hợp khác.").arg(m_hotkeyKind->currentText()));
                QSignalBlocker b(m_hotkey); m_hotkey->setChecked(false); on = false;
            }
        } else if (m_filter) {
            QCoreApplication::instance()->removeNativeEventFilter(m_filter.get()); m_filter.reset();
        }
#else
        Q_UNUSED(on)
#endif
        if (persist) {
            QSettings st("DungOffice", "Screenshot");
            st.setValue("hotkeyEnabled", m_hotkey->isChecked()); st.setValue("hotkeyKind", m_hotkeyKind->currentIndex());
        }
    }

    Canvas* m_canvas = nullptr;
    std::unique_ptr<Document> m_doc;
    QPushButton *m_bWindow = nullptr, *m_colorBtn = nullptr, *m_undo = nullptr, *m_redo = nullptr, *m_copy = nullptr, *m_save = nullptr, *m_saveAs = nullptr, *m_insert = nullptr;
    std::function<QString(const QImage&)> m_insertFn;
    QSpinBox *m_delay = nullptr, *m_width = nullptr;
    QCheckBox* m_hotkey = nullptr; QComboBox *m_hotkeyKind = nullptr, *m_fmt = nullptr;
    QButtonGroup* m_group = nullptr; QLabel* m_status = nullptr; QColor m_color;
    QVector<CaptureOverlay*> m_overlays; bool m_hadHost = false;
#ifdef Q_OS_WIN
    bool m_hkRegistered = false;
    std::unique_ptr<HotkeyFilter> m_filter;
#endif
};

} // namespace dungshot

QWidget* createScreenshotWidget(QWidget* parent) { return new dungshot::ScreenshotWidget(parent); }

void screenshotWidgetLoadImage(QWidget* w, const QImage& img) { if (auto* s = dynamic_cast<dungshot::ScreenshotWidget*>(w)) s->loadImage(img); }
QImage screenshotWidgetRender(QWidget* w) { auto* s = dynamic_cast<dungshot::ScreenshotWidget*>(w); return s ? s->render() : QImage(); }

void screenshotWidgetSetInsertHandler(QWidget* w, std::function<QString(const QImage&)> h) {
    if (auto* s = dynamic_cast<dungshot::ScreenshotWidget*>(w)) s->setInsertHandler(std::move(h));
}

#include "ScreenshotWidget.moc"
