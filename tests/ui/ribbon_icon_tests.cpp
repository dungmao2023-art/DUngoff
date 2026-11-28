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
                r.setCommandState(QStringLiteral(".uno:CharFontName"),family->itemText(0));
                r.setCommandState(QStringLiteral(".uno:FontHeight"),QStringLiteral("18"));
                if(family->currentIndex()!=0||size->currentText()!=QLatin1String("18")||!emitted.isEmpty()){
                    printf("[LỖI] trạng thái engine không đồng bộ hoặc phát lệnh khi cập nhật UI\n");++fail;
                }
                const QString reportedFont=family->itemText(0);
                const int familyCount=family->count(),sizeCount=size->count();
                for(int i=0;i<1000;++i) {
                    r.setCommandState(QStringLiteral(".uno:CharFontName"),reportedFont);
                    r.setCommandState(QStringLiteral(".uno:FontHeight"),QStringLiteral("18.0"));
                }
                if(!emitted.isEmpty()||family->count()!=familyCount||size->count()!=sizeCount||
                   family->currentText()!=reportedFont||size->currentText()!=QLatin1String("18")){
                    printf("[LỖI] repeated state changed ribbon or emitted commands\n");++fail;
                }
                // A user can change the local widget before the engine acknowledges.
                // The identical engine payload must still restore the true state.
                size->setCurrentIndex(-1);
                r.setCommandState(QStringLiteral(".uno:FontHeight"),QStringLiteral("18.0"));
                if(size->currentText()!=QLatin1String("18")||!emitted.isEmpty()){
                    printf("[LỖI] repeated acknowledgement did not restore widget state\n");++fail;
                }
                r.setCommandState(QStringLiteral(".uno:CharFontName"),QStringLiteral("DungOfficeTestMissingFont"));
                r.setCommandState(QStringLiteral(".uno:CharFontName"),QStringLiteral("DungOfficeTestMissingFont"));
                if(family->currentText()!=QLatin1String("DungOfficeTestMissingFont")||family->count()!=familyCount+1){
                    printf("[LỖI] missing font is not preserved exactly once\n");++fail;
                }
                // With only the platform inventory, an engine-reported
                // missing name is still display-only.
                QMetaObject::invokeMethod(family,"activated",Qt::DirectConnection,Q_ARG(int,family->currentIndex()));
                if(!emitted.isEmpty()){printf("[LỖI] fallback sent display-only missing font\n");++fail;}
                for(int i=0;i<30;++i)
                    r.setCommandState(QStringLiteral(".uno:CharFontName"),QStringLiteral("DungMissing%1").arg(i));
                if(family->count()!=familyCount+1){printf("[LỖI] missing font history accumulated\n");++fail;}
                r.setCommandState(QStringLiteral(".uno:CharFontName"),QString());
                if(family->count()!=familyCount||family->currentIndex()!=-1||!emitted.isEmpty()){
                    printf("[LỖI] empty font state retained missing font or emitted command\n");++fail;
                }
                r.setCommandState(QStringLiteral(".uno:CharFontName"),reportedFont);
                QMetaObject::invokeMethod(family,"activated",Qt::DirectConnection,Q_ARG(int,0));
                if(!emitted.startsWith(QLatin1String(".uno:CharFontName#"))){printf("[LỖI] chọn font không phát lệnh\n");++fail;}
                emitted.clear();
                QMetaObject::invokeMethod(size,"activated",Qt::DirectConnection,Q_ARG(int,size->currentIndex()));
                if(!emitted.startsWith(QLatin1String(".uno:FontHeight#"))){printf("[LỖI] chọn cỡ không phát lệnh\n");++fail;}
                // Simulate a verified engine inventory replacing the platform list.
                emitted.clear();
                r.setDocumentFonts({QStringLiteral("Engine Font A"),QStringLiteral("Engine Font B")});
                if(family->count()!=2||family->currentIndex()!=-1||!emitted.isEmpty()){
                    printf("[LỖI] inventory engine không thay thế danh sách Qt\n");++fail;
                }
                r.setCommandState(QStringLiteral(".uno:CharFontName"),QStringLiteral("Original Missing Font"));
                const int missing=family->currentIndex();
                if(missing<0||family->currentText()!=QLatin1String("Original Missing Font")){
                    printf("[LỖI] mất tên phông gốc\n");++fail;
                }
                QMetaObject::invokeMethod(family,"activated",Qt::DirectConnection,Q_ARG(int,missing));
                if(!emitted.isEmpty()){printf("[LỖI] gửi lệnh font chưa được engine xác minh\n");++fail;}
                QMetaObject::invokeMethod(family,"activated",Qt::DirectConnection,Q_ARG(int,0));
                if(!emitted.startsWith(QLatin1String(".uno:CharFontName#"))){printf("[LỖI] font engine không phát lệnh\n");++fail;}
                emitted.clear();
                r.setCommandState(QStringLiteral(".uno:CharFontName"),QStringLiteral("disabled"));
                QMetaObject::invokeMethod(family,"activated",Qt::DirectConnection,Q_ARG(int,0));
                if(!emitted.isEmpty()){printf("[LỖI] disabled font selector sent command\n");++fail;}
                r.setCommandState(QStringLiteral(".uno:CharFontName"),QStringLiteral("enabled"));
                r.setDocumentFonts({QStringLiteral("Other Document Font")});
                if(family->count()!=1||family->itemText(0)!=QLatin1String("Other Document Font")||!emitted.isEmpty()){
                    printf("[LỖI] font của tab cũ còn sót\n");++fail;
                }
                r.setDocumentFonts({});
                if(family->count()==0){printf("[LỖI] thiếu fallback khi engine không trả danh sách\n");++fail;}
                QObject::disconnect(connection);
            }
        }
        bool slideNew=false, animation=false, show=false, compactBold=false;
        for (auto* b : r.findChildren<QToolButton*>(QStringLiteral("RibbonButton"))) {
            ++built;
            if (b->icon().isNull() || b->icon().pixmap(QSize(22, 22)).isNull()) ++noIcon;
            const bool labeled=b->toolButtonStyle()==Qt::ToolButtonTextBesideIcon;
            if((b->toolButtonStyle()!=Qt::ToolButtonIconOnly && !(k==dung::DocKind::Presentation&&labeled))||
               b->toolTip().isEmpty()||b->accessibleName().isEmpty()||
               (labeled&&(b->text().isEmpty()||b->width()<b->sizeHint().width()))){
                printf("[LỖI] nút ribbon thiếu tooltip, sai kiểu hoặc thiếu chỗ cho nhãn\n");++fail;
            }
            const QString id=b->property("commandId").toString();
            if(id==QLatin1String(".uno:InsertPage"))slideNew=labeled;
            if(id==QLatin1String("ppt.animationCopy"))animation=labeled;
            if(id==QLatin1String("ppt.showHere"))show=labeled;
            if(id==QLatin1String(".uno:Bold"))compactBold=!labeled;
        }
        if(k==dung::DocKind::Presentation&&(!slideNew||!animation||!show||!compactBold)){
            printf("[LỖI] PowerPoint thiếu nhãn thao tác chính hoặc phông chữ không gọn\n");++fail;
        }
    }
    printf("Nút đã dựng: %d, nút không có icon: %d\n", built, noIcon);
    if (built == 0 || noIcon) { printf("[LỖI]  có nút không có icon\n"); ++fail; }
    if (!fail) printf("[ĐẠT]  mọi nút ribbon đều có icon\n");
    return fail ? 1 : 0;
}
