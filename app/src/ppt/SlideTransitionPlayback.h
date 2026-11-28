// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QImage>
#include <QtGlobal>
#include <cmath>
namespace slideshow {
enum class TransitionKind { Cut, Fade, Push, Wipe, Dissolve, SilkWave, PrismTiles, FocusLens, InkBloom, HologramScan };
enum class SignatureStyle { Native, SilkWave, PrismTiles, FocusLens, InkBloom, HologramScan };
struct TransitionPlan {
    TransitionKind kind=TransitionKind::Cut;
    int durationMs=0;
    // Direction in screen coordinates, from the incoming edge.
    int dx=0,dy=0;
};
inline TransitionPlan signaturePlan(SignatureStyle style){
    switch(style){
    case SignatureStyle::SilkWave:return {TransitionKind::SilkWave,850,0,0};
    case SignatureStyle::PrismTiles:return {TransitionKind::PrismTiles,1000,0,0};
    case SignatureStyle::FocusLens:return {TransitionKind::FocusLens,900,0,0};
    case SignatureStyle::InkBloom:return {TransitionKind::InkBloom,1100,0,0};
    case SignatureStyle::HologramScan:return {TransitionKind::HologramScan,950,0,0};
    default:return {};
    }
}
inline qreal smoothStep(qreal value){
    const qreal x=qBound(qreal(0),value,qreal(1));
    return x*x*(3-2*x);
}
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
        }else if(plan.kind==TransitionKind::SilkWave){
            // Alternating silk ribbons reveal the destination with a travelling phase.
            QPainterPath clip;
            constexpr int bands=18;
            for(int band=0;band<bands;++band){
                const qreal phase=.23*band/(bands-1);
                const qreal reveal=smoothStep((t-phase)/.77);
                const qreal w=r.width()*reveal;
                if(w<=0)continue;
                const qreal y=r.top()+band*r.height()/bands;
                const qreal x=band%2?r.right()-w:r.left();
                clip.addRect(QRectF(x,y,w,r.height()/bands+.01));
            }
            painter.setClipPath(clip,Qt::IntersectClip);painter.drawImage(r,newFrame);
        }else if(plan.kind==TransitionKind::PrismTiles){
            // Each tile flips around its centre in a diagonal wave, with a shaded edge.
            constexpr int cols=12,rows=7;
            const QRectF source(0,0,newFrame.width(),newFrame.height());
            for(int y=0;y<rows;++y)for(int x=0;x<cols;++x){
                const qreal phase=.36*(x+y)/qreal(cols+rows-2);
                const qreal f=smoothStep((t-phase)/.64);
                if(f<=0)continue;
                const QRectF cell(r.left()+x*r.width()/cols,r.top()+y*r.height()/rows,
                                  r.width()/cols,r.height()/rows);
                painter.save();painter.setClipRect(cell,Qt::IntersectClip);
                if(f<.5){
                    painter.fillRect(cell,QColor(0,0,0,qRound(65*f*2)));
                }else{
                    painter.fillRect(cell,Qt::black);
                    const qreal scale=(f-.5)*2;
                    const QRectF target(cell.center().x()-cell.width()*scale/2,cell.top(),
                                        cell.width()*scale,cell.height());
                    const QRectF src(x*source.width()/cols,y*source.height()/rows,
                                     source.width()/cols,source.height()/rows);
                    if(scale>0)painter.drawImage(target,newFrame,src);
                }
                painter.restore();
            }
        }else if(plan.kind==TransitionKind::FocusLens){
            // Gentle optical zoom, revealed by an expanding circular aperture.
            const qreal eased=smoothStep(t);
            const qreal radius=std::hypot(r.width(),r.height())*.5*eased;
            QPainterPath aperture;aperture.addEllipse(r.center(),radius,radius);
            painter.setClipPath(aperture,Qt::IntersectClip);
            const qreal scale=1.12-.12*eased;
            const QSizeF sz=r.size()*scale;
            const QRectF zoomed(r.center()-QPointF(sz.width()/2,sz.height()/2),sz);
            painter.drawImage(zoomed,newFrame);
        }else if(plan.kind==TransitionKind::InkBloom){
            // Overlapping irregular ink blooms. Use winding fill to avoid holes
            // where separate droplets meet, without per-frame randomness.
            QPainterPath ink;ink.setFillRule(Qt::WindingFill);
            const qreal extent=std::hypot(r.width(),r.height());
            for(int drop=0;drop<7;++drop){
                const qreal phase=drop*.033;
                const qreal f=smoothStep((t-phase)/(1-phase));
                if(f<=0)continue;
                const QPointF centre(r.left()+r.width()*(.12+.76*((drop*37)%7)/6.0),
                                     r.top()+r.height()*(.18+.64*((drop*13)%7)/6.0));
                QPainterPath blob;
                for(int k=0;k<=48;++k){
                    const qreal angle=k*6.283185307179586/48;
                    const qreal radius=extent*.72*f*(1+.12*std::sin(angle*5+drop)+.07*std::cos(angle*9-drop));
                    const QPointF point=centre+QPointF(radius*std::cos(angle),radius*std::sin(angle));
                    if(k==0)blob.moveTo(point);else blob.lineTo(point);
                }
                blob.closeSubpath();ink.addPath(blob);
            }
            painter.setClipPath(ink,Qt::IntersectClip);painter.drawImage(r,newFrame);
        }else if(plan.kind==TransitionKind::HologramScan){
            // A soft diagonal scanning edge reveals alternating scanline bands.
            // No flashing or periodic full-screen brightness change.
            const qreal edge=smoothStep(t)*(r.width()+r.height()*.25);
            constexpr int bands=32;
            for(int band=0;band<bands;++band){
                const qreal y=r.top()+r.height()*band/bands;
                const qreal width=qBound(qreal(0),edge-r.height()*.25*band/bands,r.width());
                if(width<=0)continue;
                const QRectF stripe(r.left(),y,width,r.height()/bands+.01);
                painter.save();painter.setClipRect(stripe,Qt::IntersectClip);painter.drawImage(r,newFrame);
                const qreal glowWidth=qMin(width,r.width()*.045);
                QLinearGradient glow(stripe.right()-glowWidth,y,stripe.right(),y);
                glow.setColorAt(0,QColor(70,220,255,0));
                glow.setColorAt(1,QColor(120,230,255,qRound(85*std::sin(t*3.141592653589793))));
                painter.fillRect(QRectF(stripe.right()-glowWidth,y,glowWidth,stripe.height()),glow);
                painter.restore();
            }
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
