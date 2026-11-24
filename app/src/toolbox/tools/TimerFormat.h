// SPDX-License-Identifier: MPL-2.0
// Định dạng/đọc thời lượng cho công cụ Đồng hồ (thuần QtCore, có kiểm thử).
#pragma once
#include <QString>
#include <QStringList>

namespace toolbox {

// 3725000 ms -> "01:02:05"; (có phần mười giây nếu withTenths) "01:02:05.0"
inline QString formatDuration(qint64 ms, bool withTenths = false) {
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const int h = int(totalSec / 3600), m = int((totalSec / 60) % 60), s = int(totalSec % 60);
    QString out = QStringLiteral("%1:%2:%3").arg(h, 2, 10, QLatin1Char('0')).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    if (withTenths) out += QStringLiteral(".%1").arg(int((ms % 1000) / 100));
    return out;
}

// "90" = 90 phút; "5:30" = 5 phút 30 giây; "1:02:03" = 1 giờ 2 phút 3 giây. Sai định dạng hoặc <= 0: trả -1. Tối đa 99 giờ.
inline qint64 parseDuration(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty()) return -1;
    const QStringList parts = t.split(QLatin1Char(':'));
    if (parts.size() > 3) return -1;
    qint64 v[3] = {0, 0, 0};
    for (int i = 0; i < parts.size(); ++i) {
        bool ok = false;
        const qint64 n = parts.at(i).trimmed().toLongLong(&ok);
        if (!ok || n < 0 || parts.at(i).trimmed().size() > 4) return -1;
        v[i] = n;
    }
    qint64 sec;
    if (parts.size() == 1) sec = v[0] * 60;
    else if (parts.size() == 2) { if (v[1] > 59) return -1; sec = v[0] * 60 + v[1]; }
    else { if (v[1] > 59 || v[2] > 59) return -1; sec = v[0] * 3600 + v[1] * 60 + v[2]; }
    if (sec <= 0 || sec > 99LL * 3600) return -1;
    return sec * 1000;
}

} // namespace toolbox
