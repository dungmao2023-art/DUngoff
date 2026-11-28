// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QStringList>
#include <QRectF>
#include <QList>
#include <QtGlobal>
#include <cmath>
namespace pptdiagram {
enum class Layout { Process, Timeline, MindMap, Cards, Hierarchy, Cycle };
struct Options {
    QString title;
    QStringList items;
    Layout layout=Layout::Process;
    int palette=0;
};
struct Node {QRectF rect;QString text;QString fill;QString ink;bool title=false;};
inline bool plan(const Options& o,QList<Node>* out){
    if(!out||o.title.trimmed().isEmpty()||o.title.size()>80||o.items.size()<2||o.items.size()>6||
       int(o.layout)<0||int(o.layout)>5||o.palette<0||o.palette>2)return false;
    for(const auto& item:o.items)if(item.trimmed().isEmpty()||item.size()>64)return false;
    const QStringList palettes[]{
        {"4338CA","0369A1","0F766E","9D174D","B45309","475569"},
        {"0F766E","047857","0369A1","155E75","166534","334155"},
        {"9D174D","B45309","7C2D12","7E22CE","BE123C","475569"}
    };
    QList<Node> result;
    auto add=[&](qreal x,qreal y,qreal w,qreal h,const QString& text,const QString& fill,const QString& ink,bool title=false){
        result.append({QRectF(x,y,w,h),text,fill,ink,title});
    };
    add(.05,.045,.90,.13,o.title.trimmed(),"172554","FFFFFF",true);
    const int n=int(o.items.size());
    if(o.layout==Layout::MindMap){
        // Two columns around a central title, with ample margins.
        add(.365,.36,.27,.28,o.title.trimmed(),"172554","FFFFFF",true);
        const int left=(n+1)/2,right=n-left;
        for(int k=0;k<n;++k){
            const bool l=k<left;const int row=l?k:k-left,count=l?left:right;
            const qreal y=.25+(.70/count)*row+(.70/count-.15)/2;
            add(l?.045:.68,y,.275,.15,o.items[k].trimmed(),palettes[o.palette][k],"FFFFFF");
        }
    }else if(o.layout==Layout::Hierarchy){
        add(.34,.25,.32,.18,o.items.first().trimmed(),"172554","FFFFFF",true);
        const int children=n-1;
        for(int k=0;k<children;++k){
            const qreal slot=.90/children;
            add(.05+k*slot+.009,.64,slot-.018,.22,o.items[k+1].trimmed(),palettes[o.palette][k],"FFFFFF");
        }
    }else if(o.layout==Layout::Cycle){
        for(int k=0;k<n;++k){
            const qreal angle=6.283185307179586*k/n-1.5707963267948966;
            const qreal x=.5+.34*std::cos(angle),y=.58+.28*std::sin(angle);
            add(x-.11,y-.068,.22,.136,o.items[k].trimmed(),palettes[o.palette][k],"FFFFFF");
        }
    }else if(o.layout==Layout::Timeline){
        for(int k=0;k<n;++k){
            const qreal slot=.90/n,x=.05+k*slot;
            add(x+.01,.27,slot-.02,.17,QString::number(k+1),palettes[o.palette][k],"FFFFFF",true);
            add(x+.01,.49,slot-.02,.35,o.items[k].trimmed(),palettes[o.palette][k],"FFFFFF");
        }
    }else{
        const int cols=o.layout==Layout::Process?qMin(3,n):2;
        const int rows=(n+cols-1)/cols;
        const qreal slotW=.90/cols,slotH=.70/rows;
        for(int k=0;k<n;++k){
            const QString label=o.layout==Layout::Process?QStringLiteral("%1. %2").arg(k+1).arg(o.items[k].trimmed()):o.items[k].trimmed();
            const int col=o.layout==Layout::Process&&(k/cols)%2?cols-1-k%cols:k%cols;
            add(.05+col*slotW+.012,.23+(k/cols)*slotH+.012,slotW-.024,slotH-.024,
                label,palettes[o.palette][k],"FFFFFF");
        }
    }
    *out=result;return true;
}
struct Edge {int from,to;QPointF start,end;bool arrow;int startSite,endSite;};
inline QList<Edge> edges(const Options& o,const QList<Node>& nodes){
    QList<Edge> result;
    // RoundRect connection sites: top, left, bottom, right.
    auto connect=[&](int from,int to,bool arrow){
        if(from<0||to<0||from>=nodes.size()||to>=nodes.size())return;
        const auto a=nodes[from].rect,b=nodes[to].rect;
        const QPointF d=b.center()-a.center();
        if(qAbs(d.x())<1e-9&&qAbs(d.y())<1e-9)return;
        auto port=[](const QRectF& r,const QPointF& dir,int* site){
            const bool horizontal=qAbs(dir.x())/r.width()>=qAbs(dir.y())/r.height();
            if(horizontal){*site=dir.x()>0?3:1;return QPointF(dir.x()>0?r.right():r.left(),r.center().y());}
            *site=dir.y()>0?2:0;return QPointF(r.center().x(),dir.y()>0?r.bottom():r.top());
        };
        Edge edge{from,to,{},{},arrow,0,0};
        edge.start=port(a,d,&edge.startSite);edge.end=port(b,-d,&edge.endSite);
        result.append(edge);
    };
    if(o.layout==Layout::MindMap||o.layout==Layout::Hierarchy){
        for(int k=2;k<nodes.size();++k)connect(1,k,false);
    }else if(o.layout==Layout::Process){
        for(int k=1;k+1<nodes.size();++k)connect(k,k+1,true);
    }else if(o.layout==Layout::Timeline){
        for(int k=1;k<nodes.size();k+=2){connect(k,k+1,false);if(k+2<nodes.size())connect(k,k+2,true);}
    }else if(o.layout==Layout::Cycle){
        for(int k=1;k<nodes.size();++k)connect(k,k+1<nodes.size()?k+1:1,true);
    }
    return result;
}
}
