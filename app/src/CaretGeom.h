// SPDX-License-Identifier: MPL-2.0
// Hình học con trỏ nhập liệu — hàm thuần (chỉ QtCore), có kiểm thử đơn vị.
//
// NGUYÊN NHÂN GỐC của lỗi "không thấy con trỏ nhấp nháy" (đo trên engine Windows thật, log CI):
// LibreOffice gửi vị trí con trỏ chữ là hình chữ nhật CHIỀU RỘNG 0, ví dụ "4469, 2204, 0, 342".
// QRect có chiều rộng 0 thì QRect::isEmpty() == true, nên mọi chỗ kiểm "!m_caret.isEmpty()" đều coi
// là KHÔNG có con trỏ và không bao giờ vẽ. Ở đây con trỏ hợp lệ khi có chiều cao (hoặc chiều rộng) dương.
#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <cmath>

namespace caretgeom {

struct Parsed {
    bool valid = false;
    QRect rect;  // twip; có thể có chiều rộng 0
};

// Chấp nhận "x, y, w, h" (có thể kèm cột/hàng phía sau), JSON {"rectangle":"x, y, w, h"} hoặc "EMPTY".
inline Parsed parseCursorPayload(const QString& payload) {
    Parsed out;
    QString s = payload.trimmed();
    if (s.startsWith(QLatin1Char('{'))) {
        const QJsonObject o = QJsonDocument::fromJson(s.toUtf8()).object();
        s = o.value(QStringLiteral("rectangle")).toString();
        if (s.isEmpty()) s = o.value(QStringLiteral("rect")).toString();
    }
    if (s.isEmpty() || s.startsWith(QLatin1String("EMPTY"))) return out;
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() < 4) return out;
    bool ok[4] = {false, false, false, false};
    const int v[4] = {parts[0].trimmed().toInt(&ok[0]), parts[1].trimmed().toInt(&ok[1]),
                      parts[2].trimmed().toInt(&ok[2]), parts[3].trimmed().toInt(&ok[3])};
    if (!(ok[0] && ok[1] && ok[2] && ok[3])) return out;
    if (v[2] < 0 || v[3] < 0 || (v[2] == 0 && v[3] == 0)) return out;  // không có kích thước nào
    out.valid = true;
    out.rect = QRect(v[0], v[1], v[2], v[3]);
    return out;
}

// Độ dày con trỏ theo điểm ảnh VẬT LÝ: 1px ở 100–149%, 2px từ 150% (rõ nhưng không dày).
inline int caretPhysicalWidth(double dpr) { return qMax(1, int(std::lround(dpr))); }

// Hình chữ nhật vẽ con trỏ (toạ độ logic của viewport) — bám lưới điểm ảnh vật lý để không bị nhoè.
// viewRect: vị trí con trỏ đã đổi sang viewport (cao >= 1, rộng thường là 1 sau khi đổi từ twip).
inline QRectF caretDeviceRect(const QRect& viewRect, double dpr) {
    if (dpr < 1.0) dpr = 1.0;
    const int pw = caretPhysicalWidth(dpr);
    const double x = std::floor(viewRect.x() * dpr) / dpr;
    const double y = std::floor(viewRect.y() * dpr) / dpr;
    const double h = qMax(std::ceil(viewRect.height() * dpr) / dpr, 2.0 / dpr);
    return QRectF(x, y, pw / dpr, h);
}

}  // namespace caretgeom
