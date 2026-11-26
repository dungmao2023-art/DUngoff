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
    } else if (id == QLatin1String("ppt.animationCopy")) {
        if (QMessageBox::question(tab, QObject::tr("Hoạt ảnh PPTX"),
            QObject::tr("Thêm hiệu ứng vào một đối tượng trong PPTX đã lưu rồi xuất bản sao. Hãy lưu bản nháp trước. Chỉ hỗ trợ slide chưa có timing; chưa preview hiệu ứng trong app. Tiếp tục?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return true;
        const QString source = QFileDialog::getOpenFileName(tab, QObject::tr("Chọn PPTX đã lưu"), d->isNew() ? QString() : fromPath(d->originalPath()), QObject::tr("PowerPoint (*.pptx)"));
        if (source.isEmpty()) return true;
        QString error;
        const auto slides = ppttransition::slideEntries(source, &error);
        if (slides.isEmpty()) { QMessageBox::warning(tab, QObject::tr("Hoạt ảnh"), error); return true; }
        bool ok = false;
        const int page = QInputDialog::getInt(tab, QObject::tr("Hoạt ảnh"), QObject::tr("Slide cần tạo hiệu ứng:"), qBound(1,d->currentPart()+1,int(slides.size())),1,int(slides.size()),1,&ok);
        if (!ok) return true;
        const auto shapes = ppttransition::slideShapes(source, page, &error);
        if (shapes.isEmpty()) { QMessageBox::warning(tab, QObject::tr("Hoạt ảnh"), error); return true; }
        QDialog dialog(tab); dialog.setWindowTitle(QObject::tr("Hoạt ảnh đối tượng — bản sao PPTX"));
        auto* root = new QVBoxLayout(&dialog); auto* form = new QFormLayout;
        auto* shape = new QComboBox;
        for (const auto& item : shapes) shape->addItem(QStringLiteral("%1 [ID %2]").arg(item.name).arg(item.id), item.id);
        auto* effect = new QComboBox; effect->addItems({QObject::tr("Xuất hiện"), QObject::tr("Mờ dần vào")});
        auto* duration = new QSpinBox; duration->setRange(100,10000); duration->setValue(500); duration->setSuffix(QStringLiteral(" ms")); duration->setEnabled(false);
        auto* delay = new QSpinBox; delay->setRange(0,30000); delay->setSuffix(QStringLiteral(" ms"));
        QObject::connect(effect, &QComboBox::currentIndexChanged, duration, [duration](int n) { duration->setEnabled(n==1); });
        form->addRow(QObject::tr("Đối tượng"),shape); form->addRow(QObject::tr("Hiệu ứng khi bấm chuột"),effect);
        form->addRow(QObject::tr("Thời lượng mờ dần"),duration); form->addRow(QObject::tr("Trễ sau khi bấm"),delay); root->addLayout(form);
        auto* note = new QLabel(QObject::tr("Chỉ một hiệu ứng trên slide chưa có animation. Kiểm tra phát lại trong PowerPoint; app chưa có trình phát animation.")); note->setWordWrap(true); root->addWidget(note);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); root->addWidget(bb);
        QObject::connect(bb,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); QObject::connect(bb,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if (dialog.exec()!=QDialog::Accepted) return true;
        const QFileInfo src(source);
        const QString dest = QFileDialog::getSaveFileName(tab,QObject::tr("Lưu PPTX mới"),src.absolutePath()+QLatin1Char('/')+src.completeBaseName()+QStringLiteral(" - hoat anh.pptx"),QObject::tr("PowerPoint (*.pptx)"));
        if (dest.isEmpty()) return true;
        ppttransition::AnimationOptions o; o.shapeId=shape->currentData().toUInt(); o.fade=effect->currentIndex()==1; o.durationMs=duration->value(); o.delayMs=delay->value();
        const auto result=ppttransition::createAnimationCopy(source,dest,page,o);
        if (!result.ok) QMessageBox::warning(tab,QObject::tr("Hoạt ảnh"),result.error);
        else QMessageBox::information(tab,QObject::tr("Hoạt ảnh"),QObject::tr("Đã tạo PPTX mới có dữ liệu animation. Kiểm tra trình chiếu trong PowerPoint. Nguồn không bị sửa."));
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
