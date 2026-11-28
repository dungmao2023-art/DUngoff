#include "SlideShow.h"
#include "SlideNavigation.h"
#include "SimpleEntrance.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <iostream>
#include <QToolTip>
#include <QInputDialog>
#include <QStringList>
#include <QPointer>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QLineF>
#include <QScreen>
#include <QWindow>
#include <QRegularExpression>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

SlideShow::SlideShow(dung::Document* doc, int startPart, QWidget* parent, bool fullscreen, QScreen* targetScreen, int intervalMs, slideshow::SignatureStyle signature)
    : QWidget(parent, Qt::Window), m_signature(signature), m_doc(doc), m_part(startPart), m_count(doc->partCount()), m_intervalMs(intervalMs) {
    m_advance.setSingleShot(true);
    m_advance.setTimerType(Qt::PreciseTimer);
    connect(&m_advance,&QTimer::timeout,this,[this]{
        if(advanceDelay()>0)go(+1);
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
    setAccessibleName(tr("Trình chiếu PowerPoint"));
    setAccessibleDescription(tr("F1: trợ giúp; G: chọn slide; L: laser; phím mũi tên: chuyển; Escape: thoát."));
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
    else { setWindowTitle(tr("Xem trước slide — F1 trợ giúp · G chọn slide · L laser · Esc đóng")); resize(960, 600); show(); }
    render();
    restartAdvance();
    setFocus(Qt::OtherFocusReason);
}

bool SlideShow::hidden(int part) {
    static const QRegularExpression re(QStringLiteral("\"visible\"\\s*:\\s*\"?0"));
    return QString::fromStdString(m_doc->partInfo(part)).contains(re);
}

bool SlideShow::hasPendingEffects() const {
    return m_entrancePart==m_part&&!m_entranceFrames.isEmpty()&&
           m_entranceStep+1<m_entranceFrames.size();
}
int SlideShow::advanceDelay() const {
    if(m_transitionTimer.isActive()||m_help||m_inkMode)return 0;
    return slideshow::autoAdvanceDelay(m_visible,m_part,m_intervalMs,m_paused,m_blank!=0,
                                       !m_digits.isEmpty(),!m_img.isNull(),hasPendingEffects());
}
void SlideShow::restartAdvance() {
    m_advance.stop();
    const int delay=advanceDelay();
    if(delay>0)m_advance.start(delay);
}

void SlideShow::go(int delta) {
    // A click during a transition finishes it, without skipping content.
    if(m_transitionTimer.isActive()){stopTransition();update();restartAdvance();return;}
    if (m_blank != 0) { m_blank = 0; update(); restartAdvance(); return; }
    if (m_part < 0 || m_count <= 0) return;
    if(m_entrancePart==m_part&&!m_entranceFrames.isEmpty()){
        const int next=m_entranceStep+delta;
        if(next>=0&&next<m_entranceFrames.size()){
            const QImage previous=m_img;
            m_entranceStep=next;m_img=m_entranceFrames[next];
            if(delta>0&&next-1<m_entrancePlans.size()){
                const auto plan=m_entrancePlans[next-1];
                if(plan.kind!=slideshow::TransitionKind::Cut){
                    const QRect bounds=slideshow::changedBounds(previous,m_img);
                    if(slideshow::isObjectMotion(plan)&&!m_motionLayers.isEmpty()){
                        m_transitionFrom=previous;m_transitionPlan=plan;
                        m_transitionClock.start();m_transitionTimer.start();
                    }else if(!slideshow::isObjectMotion(plan)&&!bounds.isEmpty()){
                        m_entranceTransitionBounds=bounds;
                        m_entranceTransitionOld=previous.copy(bounds);
                        m_entranceTransitionNew=m_img.copy(bounds);
                        m_transitionFrom=previous;m_transitionPlan=plan;
                        m_transitionClock.start();m_transitionTimer.start();
                    }
                }
            }
            update();restartAdvance();return;
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
    else if(m_entrancePart==m_part&&!m_entranceFrames.isEmpty()){
        // Returning from the next slide should show all content that has
        // already been presented, not hide it again. Back then steps through
        // the effects in reverse. A direct slide jump still starts fresh.
        m_entranceStep=int(m_entranceFrames.size())-1;
        m_img=m_entranceFrames[m_entranceStep];
    }
    update();
    restartAdvance();
}
void SlideShow::stopTransition(){
    m_transitionTimer.stop();m_transitionFrom={};m_transitionPlan={};
    m_entranceTransitionBounds={};m_entranceTransitionOld={};m_entranceTransitionNew={};
}
void SlideShow::startTransition(const QImage& previous){
    if(previous.isNull()||m_img.isNull()||previous==m_img)return;
    if(m_signature!=slideshow::SignatureStyle::Native)m_transitionPlan=slideshow::signaturePlan(m_signature);
    else{
        const auto info=QJsonDocument::fromJson(QByteArray::fromStdString(m_doc->presentationInfo())).object();
        for(const auto& item:info.value("slides").toArray()){
            const auto slide=item.toObject();
            if(slide.value("index").toInt(-1)!=m_part)continue;
            m_transitionPlan=slideshow::transitionPlan(slide);
            break;
        }
    }
    if(m_transitionPlan.kind==slideshow::TransitionKind::Cut)return;
    m_transitionFrom=previous;m_transitionClock.start();m_transitionTimer.start();
}

void SlideShow::render() {
    stopTransition();
    m_inking=false;
    if(m_inkPart!=m_part){m_ink.clear();m_inkPart=m_part;}
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
    m_entranceFrames.clear();m_entrancePlans.clear();m_motionLayers.clear();m_entranceStep=0;m_entrancePart=-1;
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
    QVector<slideshow::ClickEntrance> entrancePlans;
    if(!slideshow::findEntrancePlans(slide.value("animations").toObject().value("root").toObject(),&entrancePlans)){
        if(trace)std::cerr<<"Entrance metadata not supported"<<std::endl;
        return false;
    }
    QStringList targets;for(const auto& entry:entrancePlans)targets.append(entry.target);
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
    const bool needMotion=std::any_of(entrancePlans.cbegin(),entrancePlans.cend(),[](const auto& e){return slideshow::isObjectMotion(e.plan);});
    QVector<MotionLayer> retained;
    qint64 retainedBytes=qint64(layers.width)*layers.height*4*frames.size();
    bool motionMemoryOk=true;
    auto draw=[&](const QImage& image,int first){
        if(needMotion&&motionMemoryOk){
            retainedBytes+=qint64(image.width())*image.height()*4;
            if(retainedBytes>64LL*1024*1024)motionMemoryOk=false;
            else {
                QRect bounds;
                if(first>0&&slideshow::isObjectMotion(entrancePlans[first-1].plan)){
                    int l=image.width(),r=-1,t=image.height(),b=-1;
                    for(int y=0;y<image.height();++y){
                        const auto* pixels=reinterpret_cast<const QRgb*>(image.constScanLine(y));
                        for(int x=0;x<image.width();++x)if(pixels[x]&0xff000000u){
                            l=qMin(l,x);r=qMax(r,x);t=qMin(t,y);b=qMax(b,y);
                        }
                    }
                    if(r<l)motionMemoryOk=false;else bounds=QRect(QPoint(l,t),QPoint(r,b));
                }
                if(motionMemoryOk)retained.append(MotionLayer{image.copy(),first,bounds});
            }
        }
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
    if(needMotion&&!motionMemoryOk)return false;
    for(const auto& entry:entrancePlans)m_entrancePlans.append(entry.plan);
    m_motionLayers=std::move(retained);
    m_entranceFrames=std::move(frames);m_entranceStep=0;m_entrancePart=m_part;
    return true;
}

void SlideShow::paintObjectMotion(QPainter& painter,const QRectF& slide,qreal progress){
    painter.save();painter.setClipRect(slide,Qt::IntersectClip);painter.fillRect(slide,Qt::white);
    for(const auto& layer:m_motionLayers){
        if(layer.firstStep>m_entranceStep)continue;
        if(layer.firstStep==m_entranceStep)
            slideshow::paintMotionLayer(painter,slide,layer.bitmap,layer.bounds,m_transitionPlan,progress);
        else painter.drawImage(slide,layer.bitmap);
    }
    painter.restore();
}
void SlideShow::toggleHelp(){
    m_help=!m_help;m_digits.clear();
    if(m_help){stopTransition();m_advance.stop();m_inking=false;m_laserPoint=QPointF(-1,-1);unsetCursor();}
    else {if(m_inkMode)setCursor(Qt::CrossCursor);else if(m_laserMode||isFullScreen())setCursor(Qt::BlankCursor);restartAdvance();}
    update();
}
void SlideShow::chooseSlide(){
    const auto indices=slideshow::visibleSlideIndices(m_visible);
    if(indices.isEmpty())return;
    QStringList choices;
    for(int i:indices)choices<<tr("Slide %1").arg(i+1);
    m_advance.stop();stopTransition();m_digits.clear();m_inking=false;
    QPointer<SlideShow> safe(this);bool ok=false;
    const auto chosen=QInputDialog::getItem(this,tr("Đi đến slide"),tr("Chọn trang chiếu (không hiển thị slide đã ẩn):"),
        choices,qMax(0,int(indices.indexOf(m_part))),false,&ok);
    if(!safe)return;
    const int row=choices.indexOf(chosen);
    if(ok&&row>=0&&row<indices.size()){
        m_blank=0;
        if(m_part!=indices[row]){m_part=indices[row];render();}
    }
    update();restartAdvance();setFocus(Qt::OtherFocusReason);
}
void SlideShow::paintHelp(QPainter& p){
    p.save();
    p.fillRect(rect(),QColor(15,23,42,225));
    p.setRenderHint(QPainter::Antialiasing);
    const qreal w=qMin(qreal(width()-16),qreal(620)),h=qMin(qreal(height()-16),qreal(428));
    if(w<=0||h<=0){p.restore();return;}
    const QRectF area((width()-w)/2,(height()-h)/2,w,h);
    p.setPen(Qt::NoPen);p.setBrush(QColor("#FFFFFF"));p.drawRoundedRect(area,12,12);
    const qreal scale=qMin(w/620.0,h/428.0);
    p.translate(area.center());p.scale(scale,scale);p.translate(-310,-214);
    QFont font=p.font();font.setPixelSize(24);font.setBold(true);p.setFont(font);
    p.setPen(QColor("#172554"));p.drawText(QRectF(24,12,572,38),Qt::AlignLeft|Qt::AlignVCenter,tr("Điều khiển trình chiếu"));
    font.setPixelSize(15);font.setBold(false);p.setFont(font);
    const QList<QPair<QString,QString>> controls{
        {tr("→ / Space"),tr("Hiệu ứng hoặc slide tiếp theo")},
        {tr("←"),tr("Quay lại")},
        {tr("G"),tr("Chọn slide trong danh sách")},
        {tr("Số + Enter"),tr("Đi nhanh đến slide theo số")},
        {tr("L"),tr("Bật / tắt bút laser")},
        {tr("I"),tr("Bút vẽ · U: hoàn tác · E: xóa nét")},
        {tr("B / W"),tr("Màn hình đen / trắng")},
        {tr("T"),tr("Tạm dừng / tiếp tục tự chuyển")},
        {tr("Home / End"),tr("Slide đầu / cuối")},
        {tr("Esc"),tr("Thoát trình chiếu")}
    };
    for(int k=0;k<controls.size();++k){
        const int y=56+k*29;
        if(k%2==0)p.fillRect(QRectF(20,y,580,30),QColor("#F1F5F9"));
        p.setPen(QColor("#0F766E"));p.drawText(QRectF(31,y,160,30),Qt::AlignVCenter|Qt::AlignLeft,controls[k].first);
        p.setPen(QColor("#334155"));p.drawText(QRectF(200,y,390,30),Qt::AlignVCenter|Qt::AlignLeft,controls[k].second);
    }
    p.setPen(QColor("#475569"));p.drawText(QRectF(24,365,572,43),Qt::AlignCenter,tr("F1, Esc hoặc nhấp chuột để quay lại trình chiếu"));
    p.restore();
}
QRectF SlideShow::slideBounds() const {
    if(m_img.isNull())return {};
    const QSizeF sz=QSizeF(m_img.size()).scaled(QSizeF(size()),Qt::KeepAspectRatio);
    return QRectF(QPointF((width()-sz.width())/2,(height()-sz.height())/2),sz);
}
void SlideShow::toggleInk(){
    m_inkMode=!m_inkMode;m_inking=false;m_digits.clear();
    if(m_inkMode){
        m_laserMode=false;m_laserPoint=QPointF(-1,-1);
        setMouseTracking(false);stopTransition();setCursor(Qt::CrossCursor);
    }else if(isFullScreen())setCursor(Qt::BlankCursor);
    else unsetCursor();
    restartAdvance();update();
}
void SlideShow::appendInk(const QPointF& position,bool start){
    const QRectF slide=slideBounds();
    if(m_blank||slide.isEmpty()||!slide.contains(position)){m_inking=false;return;}
    const QPointF point((position.x()-slide.left())/slide.width(),(position.y()-slide.top())/slide.height());
    // Keep memory and event cost bounded; ink is local to this slide and session.
    if(start){
        m_inking=m_ink.size()<128;
        if(m_inking){QPolygonF stroke;stroke.append(point);m_ink.append(std::move(stroke));}
    }else if(m_inking&&!m_ink.isEmpty()){
        auto& stroke=m_ink.last();
        if(stroke.size()<2048 && QLineF(stroke.last(),point).length()>=.001)stroke.append(point);
    }
    update();
}
void SlideShow::paintInk(QPainter& painter,const QRectF& slide){
    if(m_inkPart!=m_part||m_ink.isEmpty())return;
    painter.save();painter.setClipRect(slide,Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal thickness=qMax(qreal(2),slide.width()*.004);
    auto screenPoint=[&](const QPointF& point){return slide.topLeft()+QPointF(point.x()*slide.width(),point.y()*slide.height());};
    for(const auto& stroke:m_ink){
        if(stroke.isEmpty())continue;
        QPolygonF pixels;pixels.reserve(stroke.size());
        for(const auto& point:stroke)pixels.append(screenPoint(point));
        // Contrast outline keeps red ink visible on both dark and light slides.
        for(int pass=0;pass<2;++pass){
            const QColor color=pass==0?Qt::white:QColor("#DC2626");
            const qreal width=thickness+(pass==0?2:0);
            painter.setPen(QPen(color,width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            if(pixels.size()==1)painter.drawPoint(pixels.first());else painter.drawPolyline(pixels);
        }
    }
    painter.restore();
}
void SlideShow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), m_blank == 2 ? Qt::white : Qt::black);
    if(m_help){paintHelp(p);return;}
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
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF slideRect=slideBounds();
    if(m_transitionTimer.isActive()){
        const qreal progress=qreal(m_transitionClock.elapsed())/m_transitionPlan.durationMs;
        if(slideshow::isObjectMotion(m_transitionPlan)&&!m_motionLayers.isEmpty()){
            paintObjectMotion(p,slideRect,progress);
        }else if(!m_entranceTransitionBounds.isEmpty()){
            p.drawImage(slideRect,m_transitionFrom);
            const qreal sx=slideRect.width()/m_img.width(),sy=slideRect.height()/m_img.height();
            const auto bounds=m_entranceTransitionBounds;
            const QRectF region(slideRect.left()+bounds.x()*sx,slideRect.top()+bounds.y()*sy,
                                bounds.width()*sx,bounds.height()*sy);
            slideshow::paintTransition(p,region,m_entranceTransitionOld,m_entranceTransitionNew,m_transitionPlan,progress);
        }else slideshow::paintTransition(p,slideRect,m_transitionFrom,m_img,m_transitionPlan,progress);
    }else p.drawImage(slideRect, m_img);
    paintInk(p,slideRect);
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
    if(m_help){
        if(!e->isAutoRepeat()&&(e->key()==Qt::Key_F1||e->key()==Qt::Key_Escape))toggleHelp();
        e->accept();return;
    }
    if(!(e->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))){
        if(e->key()==Qt::Key_F1){if(!e->isAutoRepeat())toggleHelp();e->accept();return;}
        if(e->key()==Qt::Key_G){if(!e->isAutoRepeat())chooseSlide();e->accept();return;}
        if(e->key()==Qt::Key_I){if(!e->isAutoRepeat())toggleInk();e->accept();return;}
        if(e->key()==Qt::Key_U||e->key()==Qt::Key_E){
            m_inking=false;
            if(!m_ink.isEmpty()){
                if(e->key()==Qt::Key_E)m_ink.clear();else m_ink.removeLast();
                update();
            }
            e->accept();return;
        }
    }
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
                if(m_inkMode)toggleInk();
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
    if(m_help){toggleHelp();e->accept();return;}
    m_digits.clear();
    if(m_inkMode&&!m_blank){
        if(e->button()==Qt::LeftButton)appendInk(e->position(),true);
        else if(e->button()==Qt::RightButton){m_inking=false;if(!m_ink.isEmpty())m_ink.removeLast();update();}
        e->accept();return;
    }
    if (e->button() == Qt::RightButton) go(-1);
    else if (e->button() == Qt::LeftButton && m_laserMode && !m_blank) {
        m_laserPoint=e->position();update();restartAdvance();e->accept();
    }
    else if (e->button() == Qt::LeftButton) go(+1);
    else { restartAdvance(); QWidget::mousePressEvent(e); }
}

void SlideShow::mouseMoveEvent(QMouseEvent* e) {
    if(m_inkMode){if(m_inking&&(e->buttons()&Qt::LeftButton))appendInk(e->position(),false);e->accept();return;}
    if(m_laserMode){m_laserPoint=e->position();update();e->accept();return;}
    QWidget::mouseMoveEvent(e);
}

void SlideShow::mouseReleaseEvent(QMouseEvent* e){
    if(m_inkMode){if(m_inking&&e->button()==Qt::LeftButton)appendInk(e->position(),false);m_inking=false;e->accept();return;}
    QWidget::mouseReleaseEvent(e);
}
void SlideShow::leaveEvent(QEvent* e) {
    m_inking=false;
    if(m_laserMode){m_laserPoint=QPointF(-1,-1);update();}
    QWidget::leaveEvent(e);
}
