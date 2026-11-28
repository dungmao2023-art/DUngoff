#include "SlideShow.h"
#include "SlideNavigation.h"
#include "SimpleEntrance.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <iostream>
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
#include <utility>

SlideShow::SlideShow(dung::Document* doc, int startPart, QWidget* parent, bool fullscreen, QScreen* targetScreen, int intervalMs)
    : QWidget(parent, Qt::Window), m_doc(doc), m_part(startPart), m_count(doc->partCount()), m_intervalMs(intervalMs) {
    m_advance.setSingleShot(true);
    m_advance.setTimerType(Qt::PreciseTimer);
    connect(&m_advance,&QTimer::timeout,this,[this]{
        if (slideshow::autoAdvanceDelay(m_visible,m_part,m_intervalMs,m_paused,m_blank!=0,
                                        !m_digits.isEmpty(),!m_img.isNull())>0) go(+1);
    });
    m_transitionTimer.setTimerType(Qt::PreciseTimer);
    m_transitionTimer.setInterval(16);
    connect(&m_transitionTimer,&QTimer::timeout,this,[this]{
        if(m_transitionClock.elapsed()>=m_transitionPlan.durationMs){stopTransition();restartAdvance();}
        update();
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
    else { setWindowTitle(tr("Xem trước slide — mũi tên chuyển trang · số + Enter đến slide · L laser · B/W màn trống · Esc đóng")); resize(960, 600); show(); }
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
    if(m_transitionTimer.isActive())return;
    const int delay=slideshow::autoAdvanceDelay(m_visible,m_part,m_intervalMs,m_paused,m_blank!=0,
                                                !m_digits.isEmpty(),!m_img.isNull());
    if(delay>0) m_advance.start(delay);
}

void SlideShow::go(int delta) {
    // A click during a transition finishes it, without skipping content.
    if(m_transitionTimer.isActive()){stopTransition();update();restartAdvance();return;}
    if (m_blank != 0) { m_blank = 0; update(); restartAdvance(); return; }
    if (m_part < 0 || m_count <= 0) return;
    if(m_entrancePart==m_part&&!m_entranceFrames.isEmpty()){
        const int next=m_entranceStep+delta;
        if(next>=0&&next<m_entranceFrames.size()){
            m_entranceStep=next;m_img=m_entranceFrames[next];update();restartAdvance();return;
        }
    }
    const int p=slideshow::adjacentVisible(m_visible,m_part,delta);
    if(p<0){
        if(delta==1){m_advance.stop();close();} // manual next ends the show
        else restartAdvance();
        return;
    }
    const QImage previous=m_img;
    m_part = p;
    render();
    if(delta>0)startTransition(previous);
    update();
    restartAdvance();
}
void SlideShow::stopTransition(){
    m_transitionTimer.stop();m_transitionFrom={};m_transitionPlan={};
}
void SlideShow::startTransition(const QImage& previous){
    if(previous.isNull()||m_img.isNull()||previous==m_img)return;
    const auto info=QJsonDocument::fromJson(QByteArray::fromStdString(m_doc->presentationInfo())).object();
    for(const auto& item:info.value("slides").toArray()){
        const auto slide=item.toObject();
        if(slide.value("index").toInt(-1)!=m_part)continue;
        m_transitionPlan=slideshow::transitionPlan(slide);
        break;
    }
    if(m_transitionPlan.kind==slideshow::TransitionKind::Cut)return;
    m_transitionFrom=previous;m_transitionClock.start();m_transitionTimer.start();
}

void SlideShow::render() {
    stopTransition();
    const int previousStep=m_entrancePart==m_part?m_entranceStep:0;
    resetEntrance();
    m_img = QImage();
    if(m_w<=0||m_h<=0)m_doc->documentSize(m_w,m_h);
    if(qEnvironmentVariableIsSet("DUNG_PPT_LAYER_TRACE"))
        std::cerr<<"Render part="<<m_part<<" count="<<m_count<<" doc="<<m_w<<"x"<<m_h<<" window="<<width()<<"x"<<height()<<std::endl;
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
    // Prefer native layers for supported animations; never require the
    // unrelated static tile renderer to succeed first.
    if(prepareEntrance()){
        m_entranceStep=qBound(0,previousStep,int(m_entranceFrames.size())-1);
        m_img=m_entranceFrames[m_entranceStep];
        return;
    }
    if(!m_doc->paintPartTile(m_img.bits(), m_part, pw, ph, 0, 0, int(m_w), int(m_h)))
        m_img=QImage(); // Stop autoplay; paintEvent shows a render-unavailable message.
}
void SlideShow::resetEntrance(){
    m_entranceFrames.clear();m_entranceStep=0;m_entrancePart=-1;
}
bool SlideShow::prepareEntrance(){
    // Avoid native layer API for unsupported animation trees.
    if(!m_doc->supportsSlideLayers())return false;
    const bool trace=qEnvironmentVariableIsSet("DUNG_PPT_LAYER_TRACE");
    const auto info=QJsonDocument::fromJson(QByteArray::fromStdString(m_doc->presentationInfo())).object();
    if(trace)std::cerr<<"Entrance prepare part="<<m_part<<" image="<<m_img.width()<<"x"<<m_img.height()<<std::endl;
    QJsonObject slide;
    for(const auto& item:info.value("slides").toArray()){
        const auto candidate=item.toObject();if(candidate.value("index").toInt(-1)==m_part){slide=candidate;break;}
    }
    QStringList targets;
    if(!slideshow::findClickEntrances(slide.value("animations").toObject().value("root").toObject(),&targets)){
        if(trace)std::cerr<<"Entrance metadata not supported"<<std::endl;
        return false;
    }
    const auto hash=slide.value("hash").toString();
    dung::Document::SlideLayers layers;
    if(!m_doc->renderSlideLayers(m_part,hash.toStdString(),qMin(1280,m_img.width()),qMin(1280,m_img.height()),layers)){
        if(trace)std::cerr<<"Native slide renderer failed"<<std::endl;
        return false;
    }
    if(trace)std::cerr<<"Native layers "<<layers.width<<"x"<<layers.height<<" rgba="<<layers.rgba<<" count="<<layers.layers.size()<<std::endl;
    // Log all synthetic-fixture metadata before applying conservative policy.
    if(trace)for(const auto& layer:layers.layers)std::cerr<<layer.json<<" bytes="<<layer.pixels.size()<<std::endl;
    if(layers.width<=0||layers.height<=0||layers.width>1280||layers.height>1280||
       qint64(layers.width)*layers.height*4*(targets.size()+1)>64LL*1024*1024)return false;
    QVector<QImage> frames;
    for(int step=0;step<=targets.size();++step){
        QImage frame(layers.width,layers.height,QImage::Format_ARGB32_Premultiplied);
        if(frame.isNull())return false;
        frame.fill(Qt::white);frames.append(std::move(frame));
    }
    auto draw=[&](const QImage& image,int first){
        for(int step=first;step<frames.size();++step){
            QPainter painter(&frames[step]);painter.drawImage(0,0,image);
        }
    };
    // TextFields are rendered separately, before their master placeholders.
    // A placeholder with no matching field bitmap represents a hidden field.
    // Resolve visible fields at their original z-order, not as top overlays.
    QHash<QString,const dung::Document::SlideLayer*> fields;
    QSet<QString> usedFields;
    const auto bytes=size_t(layers.width)*layers.height*4;
    auto parse=[](const dung::Document::SlideLayer& layer){
        return QJsonDocument::fromJson(QByteArray::fromStdString(layer.json)).object();
    };
    auto imageOf=[&](const dung::Document::SlideLayer& layer){
        return QImage(layer.pixels.data(),layers.width,layers.height,layers.width*4,
                      layers.rgba?QImage::Format_RGBA8888_Premultiplied:QImage::Format_ARGB32_Premultiplied);
    };
    for(const auto& layer:layers.layers){
        const auto o=parse(layer);
        if(o.value("slideHash").toString()!=hash)return false;
        if(o.value("group").toString()!="TextFields")continue;
        const auto content=o.value("content").toObject();
        const auto fieldHash=content.value("hash").toString();
        if(fieldHash.isEmpty()||fields.contains(fieldHash)||layer.pixels.size()!=bytes)return false;
        fields.insert(fieldHash,&layer);
    }
    QSet<QString> animatedTargets;bool background=false;
    for(const auto& layer:layers.layers){
        const auto o=parse(layer);
        const auto type=o.value("type").toString(),group=o.value("group").toString();
        const auto content=o.value("content").toObject();
        if(group=="TextFields")continue;
        if(group=="MasterPage"&&type=="placeholder"){
            const auto fieldHash=content.value("hash").toString(),fieldType=content.value("type").toString();
            if(fieldHash.isEmpty()||!layer.pixels.empty()||usedFields.contains(fieldHash))return false;
            usedFields.insert(fieldHash);
            const auto* field=fields.value(fieldHash,nullptr);
            if(field){
                if(parse(*field).value("content").toObject().value("type").toString()!=fieldType)return false;
                draw(imageOf(*field),0);
            }else if(fieldType!="DateTime"&&fieldType!="Footer"&&fieldType!="SlideNumber"){
                // Unknown missing field: refuse rather than silently lose content.
                return false;
            }
            continue;
        }
        if(layer.pixels.size()!=bytes||(type!="bitmap"&&type!="animated")){
            if(trace)std::cerr<<"Unsupported native layer type or size "<<type.toStdString()<<" "<<group.toStdString()<<std::endl;
            return false;
        }
        if(group=="Background")background=true;
        if(group!="Background"&&group!="MasterPage"&&group!="DrawPage")return false;
        const auto img=imageOf(layer);
        if(type=="animated"){
            const auto target=content.value("hash").toString();
            const int step=targets.indexOf(target);
            if(group!="DrawPage"||step<0||animatedTargets.contains(target)||
               !content.value("initVisible").isBool()||content.value("initVisible").toBool()){
                if(trace)std::cerr<<"Native animation target or visibility mismatch "<<target.toStdString()<<std::endl;
                return false;
            }
            animatedTargets.insert(target);draw(img,step+1);
        }else draw(img,0);
    }
    for(auto it=fields.constBegin();it!=fields.constEnd();++it)if(!usedFields.contains(it.key()))return false;
    if(!background||animatedTargets.size()!=targets.size())return false;
    for(int step=1;step<frames.size();++step)if(frames[step]==frames[step-1])return false;
    m_entranceFrames=std::move(frames);m_entranceStep=0;m_entrancePart=m_part;
    return true;
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
    const QRectF slideRect(QPointF((width() - sz.width()) / 2, (height() - sz.height()) / 2), sz);
    if(m_transitionTimer.isActive())
        slideshow::paintTransition(p,slideRect,m_transitionFrom,m_img,m_transitionPlan,
            qreal(m_transitionClock.elapsed())/m_transitionPlan.durationMs);
    else p.drawImage(slideRect, m_img);
    // Screen-only pointer: no engine calls, no document edits, no mark on letterbox.
    if(m_laserMode && slideRect.contains(m_laserPoint)){
        p.save();
        p.setClipRect(slideRect);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::white,1.5));
        p.setBrush(QColor(238,35,45));
        p.drawEllipse(m_laserPoint,6.0,6.0);
        p.restore();
    }
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
        if(e->key()==Qt::Key_L){
            if(!e->isAutoRepeat()){
                m_laserMode=!m_laserMode;
                m_laserPoint=QPointF(-1,-1);
                setMouseTracking(m_laserMode);
                if(m_laserMode||isFullScreen())setCursor(Qt::BlankCursor);
                else unsetCursor();
                update();
            }
            e->accept();return;
        }
        if(e->key()==Qt::Key_N || e->key()==Qt::Key_P){
            go(e->key()==Qt::Key_N?1:-1);e->accept();return;
        }
    }
    switch (e->key()) {
    case Qt::Key_Escape: m_advance.stop(); close(); break;
    case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: case Qt::Key_Space: case Qt::Key_Return: case Qt::Key_Enter: go(+1); break;
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: case Qt::Key_Backspace: go(-1); break;
    case Qt::Key_B: case Qt::Key_Period:
        stopTransition();m_blank = m_blank == 1 ? 0 : 1; update(); restartAdvance(); break;
    case Qt::Key_W: case Qt::Key_Comma:
        stopTransition();m_blank = m_blank == 2 ? 0 : 2; update(); restartAdvance(); break;
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
    else if (e->button() == Qt::LeftButton && m_laserMode && !m_blank) {
        m_laserPoint=e->position();update();restartAdvance();e->accept();
    }
    else if (e->button() == Qt::LeftButton) go(+1);
    else { restartAdvance(); QWidget::mousePressEvent(e); }
}

void SlideShow::mouseMoveEvent(QMouseEvent* e) {
    if(m_laserMode){m_laserPoint=e->position();update();e->accept();return;}
    QWidget::mouseMoveEvent(e);
}

void SlideShow::leaveEvent(QEvent* e) {
    if(m_laserMode){m_laserPoint=QPointF(-1,-1);update();}
    QWidget::leaveEvent(e);
}
