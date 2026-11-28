#include "SlideShow.h"
#include "SlideNavigation.h"
#include <QToolTip>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QRegularExpression>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>

SlideShow::SlideShow(dung::Document* doc, int startPart, QWidget* parent, bool fullscreen, QScreen* targetScreen, int intervalMs)
    : QWidget(parent, Qt::Window), m_doc(doc), m_part(startPart), m_count(doc->partCount()), m_intervalMs(intervalMs) {
    m_advance.setSingleShot(true);
    m_advance.setTimerType(Qt::PreciseTimer);
    connect(&m_advance,&QTimer::timeout,this,[this]{
        if (slideshow::autoAdvanceDelay(m_visible,m_part,m_intervalMs,m_paused,m_blank!=0,
                                        !m_digits.isEmpty(),!m_img.isNull())>0) go(+1);
    });
    setAttribute(Qt::WA_DeleteOnClose);
    // Freeze editor interactions while the show uses its document and slide count.
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Trình chiếu"));
    setFocusPolicy(Qt::StrongFocus);
    if (fullscreen) setCursor(Qt::BlankCursor);
    m_doc->documentSize(m_w, m_h);
    for(int i=0;i<m_count;++i) m_visible.append(!hidden(i));
    m_part=slideshow::firstVisible(m_visible,startPart);
    if (fullscreen) {
        if (targetScreen) {
            // Create the native window before assigning its screen, then place it
            // inside that screen's geometry before requesting fullscreen.
            winId();
            if (windowHandle()) windowHandle()->setScreen(targetScreen);
            setGeometry(targetScreen->geometry());
        }
        showFullScreen();
    }
    else { setWindowTitle(tr("Xem trước slide — mũi tên chuyển trang · số + Enter đến slide · B/W màn trống · Esc đóng")); resize(960, 600); show(); }
    render();
    restartAdvance();
    setFocus(Qt::OtherFocusReason);
}

bool SlideShow::hidden(int part) {
    static const QRegularExpression re(QStringLiteral("\"visible\"\\s*:\\s*\"?0"));
    return QString::fromStdString(m_doc->partInfo(part)).contains(re);
}

void SlideShow::restartAdvance() {
    m_advance.stop();
    const int delay=slideshow::autoAdvanceDelay(m_visible,m_part,m_intervalMs,m_paused,m_blank!=0,
                                                !m_digits.isEmpty(),!m_img.isNull());
    if(delay>0) m_advance.start(delay);
}

void SlideShow::go(int delta) {
    if (m_blank != 0) { m_blank = 0; update(); restartAdvance(); return; }
    if (m_part < 0 || m_count <= 0) return;
    const int p=slideshow::adjacentVisible(m_visible,m_part,delta);
    if(p<0){
        if(delta==1){m_advance.stop();close();} // manual next ends the show
        else restartAdvance();
        return;
    }
    m_part = p;
    render();
    update();
    restartAdvance();
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
    if (m_img.isNull()) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, tr("Không hiển thị được slide này. Dùng phím mũi tên hoặc Esc."));
        return;
    }
    const QSizeF sz = QSizeF(m_img.size()).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QRectF(QPointF((width() - sz.width()) / 2, (height() - sz.height()) / 2), sz), m_img);
}

void SlideShow::resizeEvent(QResizeEvent*) { render(); restartAdvance(); }

void SlideShow::keyPressEvent(QKeyEvent* e) {
    if (e->key()>=Qt::Key_0 && e->key()<=Qt::Key_9 &&
        !(e->modifiers() & (Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))) {
        if (m_digits.size()<9) m_digits+=QChar(ushort('0'+e->key()-Qt::Key_0));
        m_advance.stop();
        e->accept();
        return;
    }
    if ((e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter) && !m_digits.isEmpty()) {
        const int target=slideshow::jumpTo(m_visible,m_digits);
        m_digits.clear();
        if(target>=0){m_part=target;m_blank=0;render();update();}
        else QToolTip::showText(QCursor::pos(),tr("Slide không tồn tại hoặc đã ẩn."),this);
        restartAdvance();
        e->accept();
        return;
    }
    const bool wasEntering=!m_digits.isEmpty();
    m_digits.clear();
    if(wasEntering) restartAdvance();
    if (m_intervalMs>0 && e->key()==Qt::Key_T &&
        !(e->modifiers() & (Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))) {
        if(!e->isAutoRepeat()){
            m_paused=!m_paused;
            restartAdvance();
            QToolTip::showText(QCursor::pos(),m_paused?tr("Đã tạm dừng tự chuyển"):tr("Đã tiếp tục tự chuyển"),this);
        }
        e->accept();return;
    }
    if (!(e->modifiers() & (Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))) {
        if(e->key()==Qt::Key_N || e->key()==Qt::Key_P){
            go(e->key()==Qt::Key_N?1:-1);e->accept();return;
        }
    }
    switch (e->key()) {
    case Qt::Key_Escape: m_advance.stop(); close(); break;
    case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: case Qt::Key_Space: case Qt::Key_Return: case Qt::Key_Enter: go(+1); break;
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: case Qt::Key_Backspace: go(-1); break;
    case Qt::Key_B: case Qt::Key_Period:
        m_blank = m_blank == 1 ? 0 : 1; update(); restartAdvance(); break;
    case Qt::Key_W: case Qt::Key_Comma:
        m_blank = m_blank == 2 ? 0 : 2; update(); restartAdvance(); break;
    case Qt::Key_Home: case Qt::Key_End: {
        m_blank = 0;
        const int step = e->key() == Qt::Key_Home ? 1 : -1;
        int p = step > 0 ? 0 : m_count - 1;
        while (p >= 0 && p < m_count && !m_visible[p]) p += step;
        if (p >= 0 && p < m_count) { m_part = p; render(); }
        update(); restartAdvance(); break;
    }
    default: QWidget::keyPressEvent(e);
    }
}

void SlideShow::mousePressEvent(QMouseEvent* e) {
    m_digits.clear();
    if (e->button() == Qt::RightButton) go(-1);
    else if (e->button() == Qt::LeftButton) go(+1);
    else { restartAdvance(); QWidget::mousePressEvent(e); }
}
