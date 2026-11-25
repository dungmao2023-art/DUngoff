// SPDX-License-Identifier: MPL-2.0
// Mọi nút trên mọi ribbon (Word/Excel/PowerPoint/PDF, thường và chỉ-đọc) đều có icon hợp lệ. QT_QPA_PLATFORM=offscreen.
#include "Icons.h"
#include "../../app/src/Ribbon.h"

#include <QApplication>
#include <QToolButton>
#include <cstdio>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Ribbon r;
    const QStringList ids = r.commandIds();
    printf("Số mã lệnh trên ribbon: %d\n", int(ids.size()));
    int fail = 0;
    QStringList unmapped, missing;
    for (const QString& id : ids) {
        const QString n = dung::ui::iconNameForCommand(id);
        if (n.isEmpty()) unmapped << id;
        else if (!dung::ui::hasIcon(n)) missing << id + QStringLiteral("->") + n;
    }
    if (ids.size() < 80) { printf("[LỖI]  ít mã lệnh bất thường (%d)\n", int(ids.size())); ++fail; }
    if (!unmapped.isEmpty()) { printf("[LỖI]  chưa có icon cho: %s\n", qPrintable(unmapped.join(QLatin1Char(' ')))); ++fail; }
    if (!missing.isEmpty()) { printf("[LỖI]  ánh xạ tới icon không tồn tại: %s\n", qPrintable(missing.join(QLatin1Char(' ')))); ++fail; }
    // Dựng thật từng ribbon và kiểm tra nút nào cũng có icon khác rỗng
    int built = 0, noIcon = 0;
    for (auto k : {dung::DocKind::Text, dung::DocKind::Spreadsheet, dung::DocKind::Presentation, dung::DocKind::Drawing}) {
        r.setKind(k, false);
        for (auto* b : r.findChildren<QToolButton*>(QStringLiteral("RibbonButton"))) {
            ++built;
            if (b->icon().isNull() || b->icon().pixmap(QSize(22, 22)).isNull()) ++noIcon;
        }
    }
    printf("Nút đã dựng: %d, nút không có icon: %d\n", built, noIcon);
    if (built == 0 || noIcon) { printf("[LỖI]  có nút không có icon\n"); ++fail; }
    if (!fail) printf("[ĐẠT]  mọi nút ribbon đều có icon\n");
    return fail ? 1 : 0;
}
