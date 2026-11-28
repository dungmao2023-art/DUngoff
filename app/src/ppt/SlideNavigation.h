// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
#include <QSize>
#include <algorithm>
#include <limits>
#include <QVector>
namespace slideshow {
inline QSize thumbnailSize(long w,long h) {
    if(w<=0 || h<=0 || w>std::numeric_limits<int>::max() || h>std::numeric_limits<int>::max()) return {};
    const double scale=std::min(128.0/w,96.0/h);
    return QSize(std::max(1,int(w*scale)),std::max(1,int(h*scale)));
}
inline int firstVisible(const QVector<bool>& visible,int start) {
    if (visible.isEmpty()) return -1;
    if (start<0 || start>=visible.size()) start=0;
    for(int i=start;i<visible.size();++i) if(visible[i]) return i;
    for(int i=start-1;i>=0;--i) if(visible[i]) return i;
    return -1;
}
// Find the adjacent visible slide. No wrap, no hidden slides, invalid inputs rejected.
inline int adjacentVisible(const QVector<bool>& visible,int current,int direction) {
    if(current<0||current>=visible.size()||(direction!=-1&&direction!=1))return -1;
    for(int i=current+direction;i>=0&&i<visible.size();i+=direction)
        if(visible[i])return i;
    return -1;
}
// Session-only autoplay: stop on the final visible slide, never wrap.
inline int autoAdvanceDelay(const QVector<bool>& visible,int current,int intervalMs,
                            bool paused,bool blank,bool enteringNumber,bool rendered) {
    return intervalMs>=1000 && intervalMs<=600000 && !paused && !blank &&
           !enteringNumber && rendered && adjacentVisible(visible,current,1)>=0 ? intervalMs : 0;
}
inline QVector<int> visibleSlideIndices(const QVector<bool>& visible) {
    QVector<int> result;
    for(int k=0;k<visible.size();++k)if(visible[k])result.append(k);
    return result;
}
inline int jumpTo(const QVector<bool>& visible,const QString& digits) {
    bool ok=false;
    const int n=digits.toInt(&ok);
    return ok && n>=1 && n<=visible.size() && visible[n-1] ? n-1 : -1;
}
}
