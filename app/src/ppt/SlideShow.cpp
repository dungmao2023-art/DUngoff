#include "SlideShow.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QString>
#include <algorithm>

SlideShow::SlideShow(dung::Document* doc, int startPart, QWidget* parent)
    : QWidget(parent, Qt::Window), m_doc(doc), m_part(startPart), m_count(doc->partCount()) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Trình chiếu"));
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor);
    m_doc->documentSize(m_w, m_h);
    if (m_part < 0 || m_part >= m_count) m_part = 0;
    showFullScreen();
}

bool SlideShow::hidden(int part) {
    static const QRegularExpression re(QStringLiteral("\"visible\"\\s*:\\s*\"?0"));
    return QString::fromStdString(m_doc->partInfo(part)).contains(re);
}

void SlideShow::go(int delta) {
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

void SlideShow::render() {
    if (m_w <= 0 || m_h <= 0 || width() <= 0) return;
    const qreal dpr = devicePixelRatioF();
    const double s = std::min(width() * dpr / m_w, height() * dpr / m_h);
    const int pw = std::max(1, int(m_w * s)), ph = std::max(1, int(m_h * s));
    m_img = QImage(pw, ph, QImage::Format_ARGB32_Premultiplied);
    m_img.fill(Qt::white);
    m_doc->paintPartTile(m_img.bits(), m_part, pw, ph, 0, 0, (int)m_w, (int)m_h);
    m_img.setDevicePixelRatio(dpr);
}

void SlideShow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_img.isNull()) return;
    const QSizeF sz = m_img.size() / m_img.devicePixelRatio();
    p.drawImage(QPointF((width() - sz.width()) / 2, (height() - sz.height()) / 2), m_img);
}

void SlideShow::resizeEvent(QResizeEvent*) { render(); }

void SlideShow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Escape: close(); break;
    case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: case Qt::Key_Space: case Qt::Key_Return: go(+1); break;
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: case Qt::Key_Backspace: go(-1); break;
    case Qt::Key_Home: m_part = 0; render(); update(); break;
    case Qt::Key_End: m_part = m_count - 1; render(); update(); break;
    default: QWidget::keyPressEvent(e);
    }
}

void SlideShow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) go(-1); else go(+1);
}
