// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QtGlobal>
#include <cmath>
namespace slideshow {
enum class TransitionKind { Cut, Fade, Push, Wipe, Dissolve };
struct TransitionPlan {
    TransitionKind kind=TransitionKind::Cut;
    int durationMs=0;
    // Direction in screen coordinates, from the incoming edge.
    int dx=0,dy=0;
};
inline TransitionPlan transitionPlan(const QJsonObject& slide){
    TransitionPlan p;
    const auto type=slide.value("transitionType").toString();
    const auto sub=slide.value("transitionSubtype").toString();
    const auto duration=slide.value("transitionDuration");
    if(!duration.isDouble()||!std::isfinite(duration.toDouble())||duration.toDouble()<=0)return p;
    p.durationMs=qBound(1,int(qMin(10000.0,duration.toDouble())),10000);
    if(type=="Fade"&&sub=="CrossFade")p.kind=TransitionKind::Fade;
    else if(type=="Dissolve"&&(sub.isEmpty()||sub=="Default"))p.kind=TransitionKind::Dissolve;
    else if(type=="PushWipe"){
        if(sub=="FromLeft")p.dx=-1;
        else if(sub=="FromRight")p.dx=1;
        else if(sub=="FromTop")p.dy=-1;
        else if(sub=="FromBottom")p.dy=1;
        else return {};
        p.kind=TransitionKind::Push;
    }else if(type=="BarWipe"&&slide.value("transitionDirection").isBool()){
        const int sign=slide.value("transitionDirection").toBool()?-1:1;
        if(sub=="LeftToRight")p.dx=sign;
        else if(sub=="TopToBottom")p.dy=sign;
        else return {};
        p.kind=TransitionKind::Wipe;
    }else return {};
    return p;
}
// Pure renderer; input images are immutable. Unknown transitions use a cut.
inline void paintTransition(QPainter& painter,const QRectF& r,const QImage& oldFrame,
                            const QImage& newFrame,const TransitionPlan& plan,qreal progress){
    const qreal t=qBound(qreal(0),progress,qreal(1));
    painter.save();painter.setClipRect(r,Qt::IntersectClip);
    if(plan.kind==TransitionKind::Cut||oldFrame.isNull()||t>=1){
        painter.drawImage(r,newFrame);painter.restore();return;
    }
    if(t<=0){painter.drawImage(r,oldFrame);painter.restore();return;}
    if(plan.kind==TransitionKind::Push){
        const QPointF offset(plan.dx*r.width(),plan.dy*r.height());
        painter.drawImage(r.translated(-offset*t),oldFrame);
        painter.drawImage(r.translated(offset*(1-t)),newFrame);
    }else{
        painter.drawImage(r,oldFrame);
        if(plan.kind==TransitionKind::Fade){
            painter.setOpacity(t);painter.drawImage(r,newFrame);
        }else if(plan.kind==TransitionKind::Wipe){
            QRectF clip=r;
            if(plan.dx!=0){clip.setWidth(r.width()*t);if(plan.dx>0)clip.moveRight(r.right());}
            else{clip.setHeight(r.height()*t);if(plan.dy>0)clip.moveBottom(r.bottom());}
            painter.setClipRect(clip,Qt::IntersectClip);painter.drawImage(r,newFrame);
        }else if(plan.kind==TransitionKind::Dissolve){
            // Fixed deterministic permutation of 1024 cells. No per-frame random flicker.
            QPainterPath clip;
            for(int i=0;i<1024;++i){
                const int rank=(i*613+193)&1023;
                if(rank>=int(t*1024))continue;
                const qreal x=r.left()+(i%32)*r.width()/32;
                const qreal y=r.top()+(i/32)*r.height()/32;
                clip.addRect(QRectF(x,y,r.width()/32,r.height()/32));
            }
            painter.setClipPath(clip,Qt::IntersectClip);painter.drawImage(r,newFrame);
        }
    }
    painter.restore();
}
}
