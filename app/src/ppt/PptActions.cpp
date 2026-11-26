#define LOK_USE_UNSTABLE_API
#include "PptActions.h"
#include "../DocumentTab.h"
#include "../TileView.h"
#include "../EngineHost.h"
#include "SlideShow.h"
#include "PptTransition.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QFileInfo>
#include <QVBoxLayout>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <QFileDialog>
#include <QInputDialog>
#include <QColorDialog>
#include "../word/WordCommands.h"
#include <QMessageBox>
#include <QUrl>
#include <QMainWindow>
#include <QStatusBar>
#include <QToolTip>
#include <QCursor>
#include <QSignalBlocker>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QAbstractItemModel>

static void armShape(DocumentTab* tab, const char* uno) {
    auto* d = tab->document();
    if (d->readOnly()) return;
    tab->setPptNotesMode(false);
    d->waitInputIdle();
    d->command(uno);
    tab->view()->setFocus();
    const QString hint = QObject::tr("Kéo chuột trên slide để đặt đối tượng đúng vị trí và kích thước; Esc để hủy.");
    if (auto* window = qobject_cast<QMainWindow*>(tab->window()))
        window->statusBar()->showMessage(hint, 7000);
    QToolTip::showText(QCursor::pos(), hint, tab->view());
}

bool handlePptAction(DocumentTab* tab, const QString& id) {
    if (!id.startsWith(QLatin1String("ppt."))) return false;
    if (!tab || tab->kind() != dung::DocKind::Presentation) return true;
    dung::Document* d = tab->document();
    if (id == QLatin1String("ppt.show") || id == QLatin1String("ppt.showHere") || id == QLatin1String("ppt.preview")) {
        tab->setPptNotesMode(false);
        const int start = id == QLatin1String("ppt.show") ? 0 : d->currentPart();
        new SlideShow(d, start, tab, id != QLatin1String("ppt.preview"));
    } else if (id == QLatin1String("ppt.notes") || id == QLatin1String("ppt.normal")) {
        tab->setPptNotesMode(id == QLatin1String("ppt.notes"));
    } else if (id == QLatin1String("ppt.fontSize")) {
        if (d->readOnly()) return true;
        bool ok = false;
        const double size = QInputDialog::getDouble(tab, QObject::tr("Cỡ chữ"), QObject::tr("Điểm (pt):"), 24, 1, 400, 1, &ok);
        if (ok) { const auto cmd = dung::word::fontSize(size); d->command(cmd.first, cmd.second); }
        tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.textColor")) {
        if (d->readOnly()) return true;
        const auto color = QColorDialog::getColor(Qt::black, tab, QObject::tr("Màu chữ"));
        if (color.isValid()) { const auto cmd = dung::word::fontColor(color.rgb()); d->command(cmd.first, cmd.second); }
        tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.goto")) {
        const int n = d->partCount();
        if (n < 1) return true;
        bool ok = false;
        const int page = QInputDialog::getInt(tab, QObject::tr("Đến trang chiếu"), QObject::tr("Trang:"), d->currentPart() + 1, 1, n, 1, &ok);
        if (ok) tab->stepPart(page - 1 - d->currentPart());
    } else if (id == QLatin1String("ppt.textbox")) {
        armShape(tab, ".uno:Text");
    } else if (id == QLatin1String("ppt.rect")) {
        armShape(tab, ".uno:BasicShapes.rectangle");
    } else if (id == QLatin1String("ppt.image")) {
        const QString f = QFileDialog::getOpenFileName(tab, QObject::tr("Chèn ảnh"), QString(),
                                                       QObject::tr("Ảnh (*.png *.jpg *.jpeg *.bmp *.gif)"));
        if (f.isEmpty()) return true;
        d->command(".uno:InsertGraphic", dung::Document::stringArg("FileName", ss(QUrl::fromLocalFile(f).toString())));
    } else if (id == QLatin1String("ppt.masterBackground")) {
        if (QMessageBox::question(tab,QObject::tr("Nền Slide Master"),
            QObject::tr("Đổi nền của master trong PPTX đã lưu, xuất bản sao mới. Slide/layout có nền riêng sẽ không đổi. "
                        "Nền cũ của master được thay bằng màu đơn sắc trong bản sao; không chỉnh hình hoặc placeholder master. Tiếp tục?"),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return true;
        const QString source=QFileDialog::getOpenFileName(tab,QObject::tr("Chọn PPTX đã lưu"),d->isNew()?QString():fromPath(d->originalPath()),QObject::tr("PowerPoint (*.pptx)"));
        if (source.isEmpty()) return true;
        QString error;
        const auto masters=ppttransition::masterEntries(source,&error);
        if (masters.isEmpty()) { QMessageBox::warning(tab,QObject::tr("Slide Master"),error); return true; }
        bool ok=false;
        const QString entry=QInputDialog::getItem(tab,QObject::tr("Slide Master"),QObject::tr("Chọn master trong gói PPTX:"),masters,0,false,&ok);
        if (!ok) return true;
        const auto color=QColorDialog::getColor(Qt::white,tab,QObject::tr("Màu nền master"));
        if (!color.isValid()) return true;
        const QFileInfo src(source);
        const QString dest=QFileDialog::getSaveFileName(tab,QObject::tr("Lưu PPTX mới"),src.absolutePath()+QLatin1Char('/')+src.completeBaseName()+QStringLiteral(" - master.pptx"),QObject::tr("PowerPoint (*.pptx)"));
        if (dest.isEmpty()) return true;
        const auto result=ppttransition::createMasterBackgroundCopy(source,dest,entry,color.name(QColor::HexRgb).mid(1));
        if (!result.ok) QMessageBox::warning(tab,QObject::tr("Slide Master"),result.error);
        else QMessageBox::information(tab,QObject::tr("Slide Master"),QObject::tr("Đã tạo PPTX mới. Mở lại để kiểm tra các slide kế thừa màu nền; nguồn không bị sửa."));
    } else if (id == QLatin1String("ppt.animationCopy")) {
        const QString source = QFileDialog::getOpenFileName(tab, QObject::tr("Chọn PPTX đã lưu (lưu bản nháp trước)"), d->isNew() ? QString() : fromPath(d->originalPath()), QObject::tr("PowerPoint (*.pptx)"));
        if (source.isEmpty()) return true;
        QString error;
        const auto slides = ppttransition::slideEntries(source, &error);
        if (slides.isEmpty()) { QMessageBox::warning(tab, QObject::tr("Hoạt ảnh"), error); return true; }
        bool ok = false;
        const int page = QInputDialog::getInt(tab, QObject::tr("Hoạt ảnh"), QObject::tr("Slide cần tạo hiệu ứng:"), qBound(1,d->currentPart()+1,int(slides.size())),1,int(slides.size()),1,&ok);
        if (!ok) return true;
        const auto shapes = ppttransition::slideShapes(source, page, &error);
        if (shapes.isEmpty()) { QMessageBox::warning(tab, QObject::tr("Hoạt ảnh"), error); return true; }
        QDialog dialog(tab); dialog.setWindowTitle(QObject::tr("Bảng hoạt ảnh · Slide %1").arg(page)); dialog.resize(660, 530);
        dialog.setStyleSheet(QStringLiteral(R"(
QDialog{background:#F4F8FF;} QListWidget{background:white;border:1px solid #DCE5F2;border-radius:8px;padding:5px;}
QListWidget::item{padding:10px;border-radius:5px;} QListWidget::item:selected{background:#FDF0EA;color:#C45530;}
QPushButton{background:white;border:1px solid #DCE5F2;border-radius:7px;padding:7px 12px;}
QPushButton:hover{background:#FDF0EA;border-color:#C45530;} QPushButton:disabled{color:#A9B4CA;}
QPushButton#AnimationPrimary{background:#C45530;color:white;font-weight:700;}
)"));
        auto* root = new QVBoxLayout(&dialog);
        auto* heading = new QLabel(QObject::tr("1. Chọn đối tượng · 2. Chọn hiệu ứng · 3. Sắp thứ tự"), &dialog);
        heading->setStyleSheet(QStringLiteral("font-weight:700;font-size:15px;color:#17294D;")); root->addWidget(heading);
        auto* form = new QFormLayout;
        auto* shape = new QComboBox;
        for (const auto& item : shapes) shape->addItem(QStringLiteral("%1 [ID %2]").arg(item.name).arg(item.id), item.id);
        auto* effect = new QComboBox; effect->addItems({QObject::tr("Xuất hiện ngay"), QObject::tr("Mờ dần vào")}); effect->setCurrentIndex(1);
        auto* duration = new QDoubleSpinBox; duration->setRange(0.1,10.0); duration->setDecimals(1); duration->setSingleStep(0.1); duration->setValue(0.5); duration->setSuffix(QObject::tr(" giây"));
        auto* delay = new QDoubleSpinBox; delay->setRange(0,30); delay->setDecimals(1); delay->setSingleStep(0.1); delay->setSuffix(QObject::tr(" giây"));
        QObject::connect(effect, &QComboBox::currentIndexChanged, duration, [duration](int n) { duration->setEnabled(n==1); });
        form->addRow(QObject::tr("Đối tượng"),shape); form->addRow(QObject::tr("Hiệu ứng"),effect);
        form->addRow(QObject::tr("Thời lượng"),duration); form->addRow(QObject::tr("Trễ sau khi nhấp"),delay); root->addLayout(form);
        auto* buttons = new QHBoxLayout;
        auto* add = new QPushButton(QObject::tr("+ Thêm"), &dialog); add->setObjectName(QStringLiteral("AnimationPrimary"));
        auto* edit = new QPushButton(QObject::tr("Cập nhật dòng"), &dialog);
        auto* quick = new QPushButton(QObject::tr("Mờ dần lần lượt tất cả"), &dialog);
        buttons->addWidget(add); buttons->addWidget(edit); buttons->addWidget(quick); root->addLayout(buttons);
        auto* list = new QListWidget(&dialog); list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setDragDropMode(QAbstractItemView::InternalMove);
        list->setDefaultDropAction(Qt::MoveAction);
        list->setDragDropOverwriteMode(false);
        list->setDropIndicatorShown(true);
        list->setToolTip(QObject::tr("Kéo thả dòng để đổi thứ tự phát"));
        root->addWidget(list,1);
        auto* order = new QHBoxLayout;
        auto* up = new QPushButton(QObject::tr("↑ Lên"), &dialog); auto* down = new QPushButton(QObject::tr("↓ Xuống"), &dialog);
        auto* remove = new QPushButton(QObject::tr("Xóa dòng"), &dialog);
        order->addWidget(up); order->addWidget(down); order->addStretch(); order->addWidget(remove); root->addLayout(order);
        auto* note = new QLabel(QObject::tr("Kéo thả dòng để đổi thứ tự. Mỗi dòng phát khi nhấp chuột, theo thứ tự từ trên xuống. Một hiệu ứng xuất hiện cho mỗi đối tượng. Chỉ dùng slide chưa có timing; không sửa nguồn. Chưa có preview diễn hoạt trong app."), &dialog); note->setWordWrap(true); root->addWidget(note);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); bb->button(QDialogButtonBox::Ok)->setText(QObject::tr("Xuất PPTX mới…")); root->addWidget(bb);
        QList<ppttransition::AnimationOptions> effects;
        auto read = [&] {
            ppttransition::AnimationOptions o; o.shapeId=shape->currentData().toUInt(); o.fade=effect->currentIndex()==1;
            o.durationMs=qRound(duration->value()*1000); o.delayMs=qRound(delay->value()*1000); return o;
        };
        auto refresh = [&](int row) {
            { QSignalBlocker block(list); list->clear();
                for (int i=0; i<effects.size(); ++i) {
                    const auto& o=effects[i];
                    const int index=shape->findData(o.shapeId);
                    list->addItem(QObject::tr("%1. %2  ·  %3  ·  %4 s  ·  trễ %5 s")
                        .arg(i+1).arg(shape->itemText(index)).arg(o.fade ? QObject::tr("Mờ dần") : QObject::tr("Xuất hiện"))
                        .arg(o.fade ? o.durationMs/1000.0 : 0.0,0,'f',1).arg(o.delayMs/1000.0,0,'f',1));
                    list->item(i)->setData(Qt::UserRole,o.shapeId);
                }
            }
            list->setCurrentRow(row);
            bb->button(QDialogButtonBox::Ok)->setEnabled(!effects.isEmpty());
            edit->setEnabled(row>=0); remove->setEnabled(row>=0);
            up->setEnabled(row>0); down->setEnabled(row>=0 && row+1<effects.size());
        };
        // Read stable shape IDs after the model finishes moving a row; never use displayed row numbers.
        QObject::connect(list->model(), &QAbstractItemModel::rowsMoved, &dialog,
            [&](const QModelIndex&, int, int, const QModelIndex&, int) {
                QList<ppttransition::AnimationOptions> ordered;
                const int selected=list->currentRow();
                for(int i=0;i<list->count();++i) {
                    const unsigned id=list->item(i)->data(Qt::UserRole).toUInt();
                    for(const auto& o:effects) if(o.shapeId==id) {ordered.append(o);break;}
                }
                if(ordered.size()==effects.size()) effects=ordered;
                for(int i=0;i<list->count();++i) {
                    auto* item=list->item(i);
                    const QString old=item->text();
                    const int dot=old.indexOf(QStringLiteral(". "));
                    if(dot>=0)item->setText(QString::number(i+1)+old.mid(dot));
                }
                up->setEnabled(selected>0);
                down->setEnabled(selected>=0 && selected+1<effects.size());
            });
        auto duplicate = [&](unsigned id, int except) {
            for (int i=0;i<effects.size();++i) if (i!=except && effects[i].shapeId==id) return true;
            return false;
        };
        QObject::connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row) {
            edit->setEnabled(row>=0); remove->setEnabled(row>=0); up->setEnabled(row>0); down->setEnabled(row>=0 && row+1<effects.size());
            if (row<0 || row>=effects.size()) return;
            const auto& o=effects[row]; shape->setCurrentIndex(shape->findData(o.shapeId)); effect->setCurrentIndex(o.fade?1:0);
            duration->setValue(o.durationMs/1000.0); delay->setValue(o.delayMs/1000.0);
        });
        QObject::connect(add,&QPushButton::clicked,&dialog,[&] {
            const auto o=read();
            if (effects.size()>=100 || duplicate(o.shapeId,-1)) { QMessageBox::information(&dialog,QObject::tr("Hoạt ảnh"),QObject::tr("Đối tượng đã có hiệu ứng hoặc danh sách đủ 100 dòng.")); return; }
            effects.append(o); refresh(int(effects.size())-1);
        });
        QObject::connect(edit,&QPushButton::clicked,&dialog,[&] {
            const int row=list->currentRow(); if(row<0) return; const auto o=read();
            if(duplicate(o.shapeId,row)) { QMessageBox::information(&dialog,QObject::tr("Hoạt ảnh"),QObject::tr("Đối tượng đã có hiệu ứng ở dòng khác.")); return; }
            effects[row]=o; refresh(row);
        });
        QObject::connect(quick,&QPushButton::clicked,&dialog,[&] {
            if(!effects.isEmpty() && QMessageBox::question(&dialog,QObject::tr("Hoạt ảnh nhanh"),QObject::tr("Thay danh sách đang soạn bằng bộ mờ dần lần lượt?"))!=QMessageBox::Yes) return;
            effects.clear(); for(const auto& item:shapes) { if(effects.size()>=100) break; ppttransition::AnimationOptions o; o.shapeId=item.id; o.fade=true; effects.append(o); }
            refresh(0);
        });
        QObject::connect(up,&QPushButton::clicked,&dialog,[&] {const int row=list->currentRow();if(row>0){effects.swapItemsAt(row,row-1);refresh(row-1);}});
        QObject::connect(down,&QPushButton::clicked,&dialog,[&] {const int row=list->currentRow();if(row>=0 && row+1<effects.size()){effects.swapItemsAt(row,row+1);refresh(row+1);}});
        QObject::connect(remove,&QPushButton::clicked,&dialog,[&] {const int row=list->currentRow();if(row>=0){effects.removeAt(row);refresh(qMin(row,int(effects.size())-1));}});
        QObject::connect(bb,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); QObject::connect(bb,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        refresh(-1);
        if (dialog.exec()!=QDialog::Accepted || effects.isEmpty()) return true;
        const QFileInfo src(source);
        const QString dest = QFileDialog::getSaveFileName(tab,QObject::tr("Lưu PPTX mới"),src.absolutePath()+QLatin1Char('/')+src.completeBaseName()+QStringLiteral(" - hoat anh.pptx"),QObject::tr("PowerPoint (*.pptx)"));
        if (dest.isEmpty()) return true;
        const auto result=ppttransition::createAnimationSequenceCopy(source,dest,page,effects);
        if (!result.ok) QMessageBox::warning(tab,QObject::tr("Hoạt ảnh"),result.error);
        else QMessageBox::information(tab,QObject::tr("Hoạt ảnh"),QObject::tr("Đã tạo bản PPTX mới với %1 hiệu ứng. Hãy kiểm tra trình chiếu trong PowerPoint. Nguồn không bị sửa.").arg(effects.size()));
    } else if (id == QLatin1String("ppt.transitionNote")) {
        if (QMessageBox::question(tab, QObject::tr("Chuyển cảnh PPTX"),
            QObject::tr("Công cụ tạo bản sao PPTX từ tệp đã lưu, giữ nguyên tài liệu đang soạn. Hãy lưu bản nháp trước nếu muốn dùng nội dung mới nhất. Chưa chỉnh animation từng đối tượng; trình xem trong app hiện tĩnh. Tiếp tục?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return true;
        const QString source = QFileDialog::getOpenFileName(tab, QObject::tr("Chọn PPTX đã lưu"), d->isNew() ? QString() : fromPath(d->originalPath()), QObject::tr("PowerPoint (*.pptx)"));
        if (source.isEmpty()) return true;
        QString error;
        const auto slides = ppttransition::slideEntries(source, &error);
        if (slides.isEmpty()) { QMessageBox::warning(tab, QObject::tr("Chuyển cảnh"), error); return true; }
        QDialog dialog(tab); dialog.setWindowTitle(QObject::tr("Chuyển cảnh — xuất PPTX mới"));
        auto* root = new QVBoxLayout(&dialog);
        auto* form = new QFormLayout;
        auto* page = new QSpinBox; page->setRange(1, slides.size()); page->setValue(qBound(1, d->currentPart()+1, int(slides.size())));
        auto* effect = new QComboBox;
        effect->addItems({QObject::tr("Không hiệu ứng"), QObject::tr("Mờ dần"), QObject::tr("Đẩy sang trái"), QObject::tr("Quét sang trái"), QObject::tr("Hòa tan")});
        effect->setCurrentIndex(1);
        auto* speed = new QComboBox; speed->addItems({QObject::tr("Chậm"), QObject::tr("Vừa"), QObject::tr("Nhanh")}); speed->setCurrentIndex(1);
        auto* click = new QCheckBox(QObject::tr("Chuyển khi bấm chuột")); click->setChecked(true);
        auto* seconds = new QSpinBox; seconds->setRange(0, 600); seconds->setSpecialValueText(QObject::tr("Tắt tự chuyển"));
        form->addRow(QObject::tr("Slide"), page); form->addRow(QObject::tr("Hiệu ứng"), effect);
        form->addRow(QObject::tr("Tốc độ"), speed); form->addRow(click); form->addRow(QObject::tr("Tự chuyển sau (giây)"), seconds); root->addLayout(form);
        auto* note = new QLabel(QObject::tr("Chuyển cảnh cơ bản được ghi vào PPTX; mở bản xuất bằng PowerPoint để kiểm tra. Chuyển cảnh phức tạp chưa hỗ trợ sẽ bị từ chối.")); note->setWordWrap(true); root->addWidget(note);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); root->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return true;
        const QFileInfo src(source);
        const QString dest = QFileDialog::getSaveFileName(tab, QObject::tr("Lưu bản PPTX mới"), src.absolutePath() + QLatin1Char('/') + src.completeBaseName() + QStringLiteral(" - chuyen canh.pptx"), QObject::tr("PowerPoint (*.pptx)"));
        if (dest.isEmpty()) return true;
        ppttransition::Options o; o.effect = ppttransition::Effect(effect->currentIndex()); o.speed = speed->currentIndex(); o.onClick = click->isChecked(); o.autoAdvanceMs = seconds->value()*1000;
        const auto result = ppttransition::createCopy(source, dest, page->value(), o);
        if (!result.ok) QMessageBox::warning(tab, QObject::tr("Chuyển cảnh"), result.error);
        else QMessageBox::information(tab, QObject::tr("Chuyển cảnh"), QObject::tr("Đã lưu PPTX mới và kiểm cấu trúc gói:\n%1\nHãy thử trình chiếu trong PowerPoint; nguồn không bị sửa.").arg(dest));
    }
    return true;
}
