#include "SlideShow.h"
#include <QKeyEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QScreen>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>

SlideShow::SlideShow(dung::Document* doc, int startPart, QWidget* parent, bool fullscreen, QScreen* targetScreen)
    : QWidget(parent, Qt::Window), m_doc(doc), m_part(startPart), m_count(doc->partCount()) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Trình chiếu"));
    setFocusPolicy(Qt::StrongFocus);
    if (fullscreen) setCursor(Qt::BlankCursor);
    m_doc->documentSize(m_w, m_h);
    if (m_count <= 0) m_part = -1;
    else if (m_part < 0 || m_part >= m_count) m_part = 0;
    // Starting a show must not expose a hidden slide, including Show from here.
    if (m_count > 0 && hidden(m_part)) {
        int next = m_part;
        while (next < m_count && hidden(next)) ++next;
        if (next == m_count) {
            next = m_part - 1;
            while (next >= 0 && hidden(next)) --next;
        }
        m_part = next >= 0 && next < m_count ? next : -1;
    }
    if (fullscreen) {
        if (targetScreen) {
            // QWidget::setScreen establishes the native window's screen before
            // fullscreen. Move as well for virtual desktops sharing a geometry.
            setScreen(targetScreen);
            move(targetScreen->geometry().topLeft());
        }
        showFullScreen();
    } else { setWindowTitle(tr("Xem trước slide — mũi tên chuyển trang · G tới slide · B/W màn trống · Esc đóng")); resize(960, 600); show(); }
    render();
}

bool SlideShow::hidden(int part) {
    static const QRegularExpression re(QStringLiteral("\"visible\"\\s*:\\s*\"?0"));
    return QString::fromStdString(m_doc->partInfo(part)).contains(re);
}

void SlideShow::go(int delta) {
    if (m_blank != 0) { m_blank = 0; update(); return; }
    if (m_part < 0 || m_count <= 0) return;
    int p = m_part;
    for (;;) {
        p += delta;
        if (p < 0) return;
        if (p >= m_count) { close(); return; } // hết bài thì thoát
        if (!hidden(p)) break;                 // bỏ qua trang đã ẩn
    }
    m_part = p;
    render();
    update();
}

void SlideShow::jumpToSlide() {
    if (m_count <= 0) return;
    bool ok = false;
    // Use real slide numbers, including the original positions of hidden slides.
    const int page = QInputDialog::getInt(this, tr("Đến slide"), tr("Số slide (1–%1):").arg(m_count),
        std::max(1, m_part + 1), 1, m_count, 1, &ok);
    if (!ok) return;
    const int target = page - 1;
    if (m_doc->partCount() != m_count) {
        QMessageBox::information(this, tr("Trình chiếu"), tr("Số slide đã thay đổi. Hãy mở lại trình chiếu."));
        close(); return;
    }
    if (hidden(target)) {
        QMessageBox::information(this, tr("Đến slide"), tr("Slide %1 đang ẩn. Chọn một slide khác.").arg(page));
        return;
    }
    m_blank = 0;
    m_part = target;
    render();
    update();
}

void SlideShow::render() {
    m_img = QImage();
    if (m_part < 0 || m_part >= m_count || m_w <= 0 || m_h <= 0 ||
        m_w > std::numeric_limits<int>::max() || m_h > std::numeric_limits<int>::max() ||
        width() <= 0 || height() <= 0) return;
    // Bound the backing bitmap even on high-DPI/large multi-monitor setups.
    const qreal dpr = devicePixelRatioF();
    const double s = std::min({width() * dpr / m_w, height() * dpr / m_h,
                               4096.0 / m_w, 4096.0 / m_h});
    if (!std::isfinite(s) || s <= 0) return;
    const int pw = std::max(1, int(m_w * s)), ph = std::max(1, int(m_h * s));
    m_img = QImage(pw, ph, QImage::Format_ARGB32_Premultiplied);
    if (m_img.isNull()) return;
    m_img.fill(Qt::white);
    m_doc->paintPartTile(m_img.bits(), m_part, pw, ph, 0, 0, int(m_w), int(m_h));
}

void SlideShow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), m_blank == 2 ? Qt::white : Qt::black);
    if (m_blank) return;
    if (m_part < 0) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, tr("Không có slide hiển thị. Esc để thoát."));
        return;
    }
    if (m_img.isNull()) return;
    const QSizeF sz = QSizeF(m_img.size()).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QRectF(QPointF((width() - sz.width()) / 2, (height() - sz.height()) / 2), sz), m_img);
}

void SlideShow::resizeEvent(QResizeEvent*) { render(); }

void SlideShow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Escape: close(); break;
    case Qt::Key_G: if (!e->isAutoRepeat()) jumpToSlide(); break;
    case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: case Qt::Key_Space: case Qt::Key_Return: go(+1); break;
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: case Qt::Key_Backspace: go(-1); break;
    case Qt::Key_B: case Qt::Key_Period:
        m_blank = m_blank == 1 ? 0 : 1; update(); break;
    case Qt::Key_W: case Qt::Key_Comma:
        m_blank = m_blank == 2 ? 0 : 2; update(); break;
    case Qt::Key_Home: case Qt::Key_End: {
        m_blank = 0;
        const int step = e->key() == Qt::Key_Home ? 1 : -1;
        int p = step > 0 ? 0 : m_count - 1;
        while (p >= 0 && p < m_count && hidden(p)) p += step;
        if (p >= 0 && p < m_count) { m_part = p; render(); }
        update(); break;
    }
    default: QWidget::keyPressEvent(e);
    }
}

void SlideShow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) go(-1);
    else if (e->button() == Qt::LeftButton) go(+1);
    else QWidget::mousePressEvent(e);
}
