// SPDX-License-Identifier: MPL-2.0
// Mọi nút trên mọi ribbon (Word/Excel/PowerPoint/PDF, thường và chỉ-đọc) đều có icon hợp lệ. QT_QPA_PLATFORM=offscreen.
#include "Icons.h"
#include "../../app/src/Ribbon.h"

#include <QApplication>
#include <QToolButton>
#include <QComboBox>
#include <QEvent>
#include <QTabBar>
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
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        if(k!=dung::DocKind::Drawing){
            auto* family=r.findChild<QComboBox*>(QStringLiteral("RibbonFontFamily"));
            auto* size=r.findChild<QComboBox*>(QStringLiteral("RibbonFontSize"));
            if(!family||!size||family->count()==0||size->count()==0){printf("[LỖI] thiếu bộ chọn font/cỡ chữ\n");++fail;}
            else{
                QString emitted;
                const auto connection=QObject::connect(&r,&Ribbon::unoCommand,&r,[&](const QString& cmd){emitted=cmd;});
                r.setFontFamilies({QStringLiteral("Carlito"),QStringLiteral("Liberation Serif")});
                if(family->count()!=2||family->itemText(0)!=QLatin1String("Carlito")||!emitted.isEmpty()){
                    printf("[LỖI] danh sách font engine chưa đồng bộ\n");++fail;
                }
                r.setCommandState(QStringLiteral(".uno:CharFontName"),family->itemText(0));
                r.setCommandState(QStringLiteral(".uno:FontHeight"),QStringLiteral("18"));
                if(family->currentIndex()!=0||size->currentText()!=QLatin1String("18")||!emitted.isEmpty()){
                    printf("[LỖI] trạng thái engine không đồng bộ hoặc phát lệnh khi cập nhật UI\n");++fail;
                }
                QMetaObject::invokeMethod(family,"activated",Qt::DirectConnection,Q_ARG(int,0));
                if(!emitted.startsWith(QLatin1String(".uno:CharFontName#"))){printf("[LỖI] chọn font không phát lệnh\n");++fail;}
                emitted.clear();
                QMetaObject::invokeMethod(size,"activated",Qt::DirectConnection,Q_ARG(int,size->currentIndex()));
                if(!emitted.startsWith(QLatin1String(".uno:FontHeight#"))){printf("[LỖI] chọn cỡ không phát lệnh\n");++fail;}
                QObject::disconnect(connection);
            }
        }
        for (auto* b : r.findChildren<QToolButton*>(QStringLiteral("RibbonButton"))) {
            ++built;
            if (b->icon().isNull() || b->icon().pixmap(QSize(22, 22)).isNull()) ++noIcon;
            if(b->toolButtonStyle()!=Qt::ToolButtonIconOnly||b->toolTip().isEmpty()||b->accessibleName().isEmpty()){
                printf("[LỖI] nút ribbon thiếu tooltip hoặc không ở chế độ icon\n");++fail;
            }
        }
    }
    printf("Nút đã dựng: %d, nút không có icon: %d\n", built, noIcon);
    if (built == 0 || noIcon) { printf("[LỖI]  có nút không có icon\n"); ++fail; }
    if (!fail) printf("[ĐẠT]  mọi nút ribbon đều có icon\n");
    return fail ? 1 : 0;
}
