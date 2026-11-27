// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QString>
#include <QVector>
namespace slideshow {
inline int firstVisible(const QVector<bool>& visible,int start) {
    if (visible.isEmpty()) return -1;
    if (start<0 || start>=visible.size()) start=0;
    for(int i=start;i<visible.size();++i) if(visible[i]) return i;
    for(int i=start-1;i>=0;--i) if(visible[i]) return i;
    return -1;
}
inline int jumpTo(const QVector<bool>& visible,const QString& digits) {
    bool ok=false;
    const int n=digits.toInt(&ok);
    return ok && n>=1 && n<=visible.size() && visible[n-1] ? n-1 : -1;
}
}
