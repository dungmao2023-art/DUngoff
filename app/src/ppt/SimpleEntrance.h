// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QSet>
#include <QVector>
#include <cmath>
#include "SlideTransitionPlayback.h"
namespace slideshow {
struct ClickEntrance {QString target;TransitionPlan plan;};
// Accept one entire-shape entrance per click. Other timing trees are left
// intact and rendered statically; do not approximate unsupported animations.
inline bool findEntrancePlans(const QJsonObject& root,QVector<ClickEntrance>* output){
    if(!output)return false;
    output->clear();
    struct Group {QString target,filterTarget;TransitionPlan plan;bool hasSet=false,hasFilter=false;QVector<QJsonObject> motion;};
    QVector<Group> groups;QSet<QString> seen;
    int nodes=0;bool valid=true;
    auto visit=[&](auto&& self,const QJsonObject& node,int depth,int click)->void{
        if(!valid||depth>32||++nodes>128){valid=false;return;}
        const auto type=node.value("nodeType").toString(),kind=node.value("nodeName").toString();
        if(type=="InteractiveSequence"||type=="WithPrevious"||type=="AfterPrevious"||
           node.contains("repeatCount")||node.contains("repeatDur")||node.contains("autoreverse")){
            valid=false;return;
        }
        if(type=="OnClick"){
            if(click>=0||groups.size()>=8){valid=false;return;}
            click=int(groups.size());groups.append(Group{});
        }
        const auto begin=node.value("begin").toString();
        if(click>=0&&!begin.isEmpty()&&begin!="0s"&&begin!="0.0s"&&begin!="0"){
            valid=false;return;
        }
        const auto children=node.value("children");
        if(kind=="Par"||kind=="Seq"){
            if(!children.isArray()||children.toArray().isEmpty()){valid=false;return;}
            for(const auto& child:children.toArray()){
                if(!child.isObject()){valid=false;return;}
                self(self,child.toObject(),depth+1,click);
            }
            return;
        }
        const auto sub=node.value("subItem").toString(),target=node.value("targetElement").toString();
        if(click<0||target.isEmpty()||(!sub.isEmpty()&&sub!="AsWhole")){valid=false;return;}
        auto& g=groups[click];
        if(kind=="Set"){
            const auto name=node.value("attributeName").toString(),value=node.value("to").toString().toLower();
            if(g.hasSet||name!="Visibility"||(value!="visible"&&value!="true")||seen.contains(target)){
                valid=false;return;
            }
            g.target=target;g.hasSet=true;seen.insert(target);
        }else if(kind=="TransitionFilter"){
            if(g.hasFilter||node.value("transitionMode").toString()=="out"){valid=false;return;}
            QString seconds=node.value("dur").toString();
            if(!seconds.endsWith('s')){valid=false;return;}
            seconds.chop(1);bool ok=false;const double duration=seconds.toDouble(&ok);
            if(!ok||!std::isfinite(duration)||duration<.1||duration>10){valid=false;return;}
            const auto filterType=node.value("transitionType").toString();
            const auto filterSub=node.value("transitionSubType").toString();
            const auto direction=node.value("transitionDirection").toString();
            if(!direction.isEmpty()&&direction!="reverse"){valid=false;return;}
            const QJsonObject metadata{{"transitionType",filterType},{"transitionSubtype",filterSub},
                {"transitionDirection",direction!="reverse"},{"transitionDuration",duration*1000}};
            const auto plan=transitionPlan(metadata);
            if(plan.kind==TransitionKind::Cut||plan.kind==TransitionKind::Push){
                valid=false;return;
            }
            g.plan=plan;g.filterTarget=target;g.hasFilter=true;
        }else if(kind=="Animate"){
            if(g.motion.size()>=4||g.hasFilter||node.contains("formula")||node.contains("keySplines")||
               node.contains("accumulate")||node.contains("accelerate")||node.contains("decelerate")){
                valid=false;return;
            }
            const auto mode=node.value("calcMode").toString(),additive=node.value("additive").toString();
            if((!mode.isEmpty()&&mode!="Linear")||(!additive.isEmpty()&&additive!="Replace")){
                valid=false;return;
            }
            g.motion.append(node);
        }else valid=false;
    };
    visit(visit,root,0,-1);
    if(!valid||groups.isEmpty())return false;
    QVector<ClickEntrance> candidate;
    for(const auto& g:groups){
        if(!g.hasSet||(g.hasFilter&&g.filterTarget!=g.target)||(!g.motion.isEmpty()&&g.hasFilter))return false;
        auto plan=g.plan;
        if(!g.motion.isEmpty()){
            QSet<QString> properties;int durationMs=0;bool width=false,height=false;int dx=0,dy=0;
            for(const auto& node:g.motion){
                if(node.value("targetElement").toString()!=g.target)return false;
                const auto attr=node.value("attributeName").toString().toLower();
                if(properties.contains(attr))return false;
                properties.insert(attr);
                QString values=node.value("values").toString().toLower();
                values.remove(' ');values.remove('#');
                QString times=node.value("keyTimes").toString();times.remove(' ');
                if(!times.isEmpty()&&times!="0;1")return false;
                QString seconds=node.value("dur").toString();if(!seconds.endsWith('s'))return false;
                seconds.chop(1);bool ok=false;const auto d=seconds.toDouble(&ok);
                if(!ok||!std::isfinite(d)||d<.1||d>10)return false;
                const int ms=qRound(d*1000);if(durationMs&&ms!=durationMs)return false;durationMs=ms;
                if(attr=="x"){
                    if(values=="0-width/2;x")dx=-1;else if(values=="1+width/2;x")dx=1;
                    else if(values!="x;x")return false;
                }else if(attr=="y"){
                    if(values=="0-height/2;y")dy=-1;else if(values=="1+height/2;y")dy=1;
                    else if(values!="y;y")return false;
                }else if(attr=="width"&&values=="0;width")width=true;
                else if(attr=="height"&&values=="0;height")height=true;
                else return false;
            }
            if(width&&height&&properties.size()==2)plan={TransitionKind::Zoom,durationMs,0,0};
            else if(!width&&!height&&(dx||dy))plan={TransitionKind::Fly,durationMs,dx,dy};
            else return false;
        }
        candidate.append(ClickEntrance{g.target,plan});
    }
    *output=candidate;return true;
}
inline bool findClickEntrances(const QJsonObject& root,QStringList* targets){
    if(!targets)return false;
    targets->clear();QVector<ClickEntrance> plans;
    if(!findEntrancePlans(root,&plans))return false;
    QStringList candidate;
    for(const auto& plan:plans){if(plan.plan.kind!=TransitionKind::Cut)return false;candidate.append(plan.target);}
    *targets=candidate;return true;
}
inline bool findSimpleEntrance(const QJsonObject& root,QString* target){
    if(!target)return false;
    target->clear();QStringList targets;
    if(!findClickEntrances(root,&targets)||targets.size()!=1)return false;
    *target=targets.first();return true;
}
// The frames are fully composited in native z-order. Restrict spatial masks
// to the pixels actually changed by the entrance, not the whole slide.
inline QRect changedBounds(const QImage& a,const QImage& b){
    if(a.size()!=b.size()||a.isNull()||a.format()!=QImage::Format_ARGB32_Premultiplied||a.format()!=b.format())return {};
    int left=a.width(),right=-1,top=a.height(),bottom=-1;
    for(int y=0;y<a.height();++y){
        const auto* aa=reinterpret_cast<const QRgb*>(a.constScanLine(y));
        const auto* bb=reinterpret_cast<const QRgb*>(b.constScanLine(y));
        for(int x=0;x<a.width();++x)if(aa[x]!=bb[x]){
            left=qMin(left,x);right=qMax(right,x);top=qMin(top,y);bottom=qMax(bottom,y);
        }
    }
    return right>=left?QRect(QPoint(left,top),QPoint(right,bottom)):QRect();
}
}
