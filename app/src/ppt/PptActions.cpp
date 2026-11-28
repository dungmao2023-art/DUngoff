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
#include <QSet>
#include <QPointer>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QScreen>
#include <functional>

// QListWidget may implement InternalMove by moving rows or by insert/remove.
// Synchronize once after the complete drag operation, not from intermediate model signals.
class AnimationOrderList final : public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<void()> orderChanged;
protected:
    void startDrag(Qt::DropActions actions) override {
        { QSignalBlocker block(this); QListWidget::startDrag(actions); }
        if(orderChanged)orderChanged();
    }
};

// A modal dialog may process queued engine callbacks. Revalidate the original
// slide and selection before issuing a command that changes the document.
struct PptEditTarget {
    QPointer<DocumentTab> tab;
    dung::Document* doc;
    int part, count;
    std::string name, graphic, text, cursor, selected;
    explicit PptEditTarget(DocumentTab* t) : tab(t), doc(t->document()) {
        doc->waitInputIdle();
        part = doc->currentPart();
        count = doc->partCount();
        if (part >= 0 && part < count) name = doc->partName(part);
        graphic = doc->lastPayload(LOK_CALLBACK_GRAPHIC_SELECTION);
        text = doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION);
        cursor = doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR);
        selected = doc->selectionAs("text/plain;charset=utf-8");
    }
    bool verify(bool requireSelection) const {
        if (!tab || tab->document() != doc || tab->readOnly() || !tab->view()) return false;
        doc->waitInputIdle();
        if (part >= 0 && part < count && doc->partCount() == count &&
            doc->currentPart() == part && doc->partName(part) == name &&
            (!requireSelection || ((!graphic.empty() || !text.empty() || !cursor.empty()) &&
                doc->lastPayload(LOK_CALLBACK_GRAPHIC_SELECTION) == graphic &&
                doc->lastPayload(LOK_CALLBACK_TEXT_SELECTION) == text &&
                doc->lastPayload(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR) == cursor &&
                doc->selectionAs("text/plain;charset=utf-8") == selected))) return true;
        QMessageBox::warning(tab.data(), QObject::tr("Vị trí chỉnh sửa"),
            QObject::tr("Không xác minh được slide hoặc vùng chọn ban đầu. Chưa thay đổi tài liệu."));
        return false;
    }
};

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
    if (id == QLatin1String("ppt.show") || id == QLatin1String("ppt.showHere") || id == QLatin1String("ppt.preview") || id == QLatin1String("ppt.showTimed")) {
        const bool fullscreen=id!=QLatin1String("ppt.preview");
        QPointer<DocumentTab> safeTab(tab);
        int intervalMs=0;
        if(id==QLatin1String("ppt.showTimed")){
            bool ok=false;
            const int seconds=QInputDialog::getInt(tab,QObject::tr("Tự chuyển slide"),
                QObject::tr("Số giây mỗi slide (không đổi thời gian trong PPTX). T: tạm dừng/tiếp tục, B/W: màn trống, Esc: thoát. Dừng ở slide cuối."),
                5,1,600,1,&ok);
            if(!ok||!safeTab||safeTab->document()!=d)return true;
            intervalMs=seconds*1000;
        }
        QPointer<QScreen> screen=safeTab->screen();
        const auto screens=QGuiApplication::screens();
        if(fullscreen && screens.size()>1) {
            QStringList labels;
            QList<QPointer<QScreen>> candidates;
            int selected=0;
            for(auto* s:screens) {
                if(s==screen.data()) selected=int(labels.size());
                labels.append(QObject::tr("%1. %2 (%3 × %4)").arg(labels.size()+1).arg(s->name())
                    .arg(s->geometry().width()).arg(s->geometry().height()));
                candidates.append(QPointer<QScreen>(s));
            }
            bool ok=false;
            const QString picked=QInputDialog::getItem(tab,QObject::tr("Màn hình trình chiếu"),
                QObject::tr("Chọn màn hình hoặc máy chiếu:"),labels,selected,false,&ok);
            if(!ok || !safeTab || safeTab->document()!=d) return true;
            const int idx=labels.indexOf(picked);
            if(idx<0 || !candidates[idx]) {
                QMessageBox::warning(safeTab.data(),QObject::tr("Trình chiếu"),QObject::tr("Màn hình đã ngắt kết nối. Hãy chọn lại."));
                return true;
            }
            screen=candidates[idx];
        }
        if(!safeTab || safeTab->document()!=d) return true;
        safeTab->setPptNotesMode(false);
        d->waitInputIdle();
        const int start=(id==QLatin1String("ppt.show")||id==QLatin1String("ppt.showTimed"))?0:d->currentPart();
        new SlideShow(d,start,safeTab.data(),fullscreen,screen.data(),intervalMs);
    } else if (id == QLatin1String("ppt.notes") || id == QLatin1String("ppt.normal")) {
        tab->setPptNotesMode(id == QLatin1String("ppt.notes"));
    } else if (id == QLatin1String("ppt.deleteSlide")) {
        if (tab->readOnly()) return true;
        tab->setPptNotesMode(false);
        const PptEditTarget target(tab);
        if (target.count<=1) {
            QMessageBox::information(tab,QObject::tr("Xóa slide"),QObject::tr("Không thể xóa slide duy nhất."));
            return true;
        }
        if(target.part<0||target.part>=target.count)return true;
        const auto hash=d->partHash(target.part);
        const auto info=d->partInfo(target.part);
        if(hash.empty() || info.empty()){
            QMessageBox::warning(tab,QObject::tr("Xóa slide"),QObject::tr("Engine chưa cung cấp đủ thông tin nhận diện slide. Chưa xóa để tránh nhầm dữ liệu."));
            return true;
        }
        if(QMessageBox::question(tab,QObject::tr("Xóa slide"),
            QObject::tr("Xóa slide %1/%2 (%3), gồm toàn bộ đối tượng và ghi chú của slide này?").arg(target.part+1).arg(target.count).arg(qs(target.name)),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return true;
        if(!target.verify(false))return true;
        if(d->partHash(target.part)!=hash||d->partInfo(target.part)!=info){
            QMessageBox::warning(target.tab.data(),QObject::tr("Xóa slide"),QObject::tr("Slide đã thay đổi. Chưa xóa."));
            return true;
        }
        d->command(".uno:DeletePage");
        d->waitInputIdle();
        if(target.tab&&target.tab->document()==d&&target.tab->view())target.tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.fontSize")) {
        if (d->readOnly()) return true;
        const PptEditTarget target(tab);
        bool ok = false;
        const double size = QInputDialog::getDouble(tab, QObject::tr("Cỡ chữ"), QObject::tr("Điểm (pt):"), 24, 1, 400, 1, &ok);
        if (ok && target.verify(true)) { const auto cmd = dung::word::fontSize(size); d->command(cmd.first, cmd.second); }
        if (target.tab && target.tab->view()) target.tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.textColor")) {
        if (d->readOnly()) return true;
        const PptEditTarget target(tab);
        const auto color = QColorDialog::getColor(Qt::black, tab, QObject::tr("Màu chữ"));
        if (color.isValid() && target.verify(true)) {
            const auto cmd = dung::word::fontColor(color.rgb() & 0x00FFFFFFu);
            d->command(cmd.first, cmd.second);
        }
        if (target.tab && target.tab->view()) target.tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.goto")) {
        d->waitInputIdle();
        QPointer<DocumentTab> safeTab(tab);
        const int n = d->partCount();
        if (n < 1) return true;
        bool ok = false;
        const int page = QInputDialog::getInt(tab, QObject::tr("Đến trang chiếu"), QObject::tr("Trang:"), d->currentPart() + 1, 1, n, 1, &ok);
        if (!ok||!safeTab||safeTab->document()!=d)return true;
        d->waitInputIdle();
        if(d->partCount()!=n){
            QMessageBox::warning(safeTab.data(),QObject::tr("Đến trang chiếu"),QObject::tr("Số slide đã thay đổi. Hãy chọn lại."));
            return true;
        }
        safeTab->stepPart(page - 1 - d->currentPart());
    } else if (id == QLatin1String("ppt.resizeObject")) {
        if(d->readOnly()||tab->pptNotesMode())return true;
        // Only the native graphic-selection bounds are used; never infer size
        // from the canvas zoom or from an unrelated text selection.
        const PptEditTarget target(tab);
        static const QRegularExpression rect(QStringLiteral("^\\s*-?\\d+\\s*,\\s*-?\\d+\\s*,\\s*([1-9]\\d*)\\s*,\\s*([1-9]\\d*)(?:\\s*,|\\s*$)"));
        const auto bounds=rect.match(QString::fromStdString(target.graphic));
        bool wOk=false,hOk=false;
        const int w=bounds.captured(1).toInt(&wOk),h=bounds.captured(2).toInt(&hOk);
        if(!bounds.hasMatch()||!wOk||!hOk||w>100000||h>100000||!target.selected.empty()){
            QMessageBox::information(tab,QObject::tr("Đổi kích thước"),QObject::tr("Chọn hình hoặc ảnh trên slide, không ở chế độ sửa chữ."));
            return true;
        }
        QDialog dlg(tab);
        dlg.setWindowTitle(QObject::tr("Đổi kích thước đối tượng"));
        auto* layout=new QVBoxLayout(&dlg);
        auto* form=new QFormLayout;
        auto* width=new QDoubleSpinBox(&dlg);
        auto* height=new QDoubleSpinBox(&dlg);
        for(auto* field:{width,height}){
            field->setRange(0.1,100.0);field->setDecimals(2);field->setSuffix(QObject::tr(" cm"));
        }
        width->setValue(w*2.54/1440.0);height->setValue(h*2.54/1440.0);
        auto* lock=new QCheckBox(QObject::tr("Giữ tỷ lệ vùng chọn"),&dlg);lock->setChecked(true);
        form->addRow(QObject::tr("Rộng:"),width);form->addRow(QObject::tr("Cao:"),height);
        layout->addLayout(form);layout->addWidget(lock);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dlg);
        layout->addWidget(buttons);
        QObject::connect(buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);
        QObject::connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        QObject::connect(width,&QDoubleSpinBox::valueChanged,&dlg,[&](double v){
            if(lock->isChecked()){QSignalBlocker b(height);height->setValue(v*double(h)/w);}
        });
        QObject::connect(height,&QDoubleSpinBox::valueChanged,&dlg,[&](double v){
            if(lock->isChecked()){QSignalBlocker b(width);width->setValue(v*double(w)/h);}
        });
        if(w*2.54/1440.0>100.0||h*2.54/1440.0>100.0)return true;
        if(dlg.exec()!=QDialog::Accepted||!target.verify(true)||tab->pptNotesMode())return true;
        const int newW=qRound(width->value()*1440.0/2.54),newH=qRound(height->value()*1440.0/2.54);
        if(lock->isChecked()&&qAbs(double(newW)/w-double(newH)/h)>0.01){
            QMessageBox::warning(tab,QObject::tr("Đổi kích thước"),QObject::tr("Kích thước vượt giới hạn giữ tỷ lệ. Chưa sửa tài liệu."));
            return true;
        }
        if(qAbs(newW-w)<=3&&qAbs(newH-h)<=3)return true;
        // LOK converts the transform dimensions from twips into engine units.
        d->command(".uno:TransformDialog",dung::word::join({
            dung::word::argLong("TransformWidth",newW),dung::word::argLong("TransformHeight",newH)}));
        d->waitInputIdle();tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.alignObjects")) {
        if(d->readOnly())return true;
        const PptEditTarget target(tab);
        static const QRegularExpression rect(QStringLiteral("^\\s*-?\\d+\\s*,\\s*-?\\d+\\s*,\\s*[1-9]\\d*\\s*,\\s*[1-9]\\d*(?:\\s*,|\\s*$)"));
        if(!rect.match(QString::fromStdString(target.graphic)).hasMatch()||!target.selected.empty()){
            QMessageBox::information(tab,QObject::tr("Căn đối tượng"),QObject::tr("Chọn các đối tượng cần căn trên slide (Shift + click để chọn nhiều hình), không sửa chữ."));
            return true;
        }
        const QStringList labels{QObject::tr("Trái"),QObject::tr("Giữa ngang"),QObject::tr("Phải"),
            QObject::tr("Trên"),QObject::tr("Giữa dọc"),QObject::tr("Dưới")};
        const QStringList commands{QStringLiteral(".uno:ObjectAlignLeft"),QStringLiteral(".uno:AlignCenter"),
            QStringLiteral(".uno:ObjectAlignRight"),QStringLiteral(".uno:AlignUp"),
            QStringLiteral(".uno:AlignMiddle"),QStringLiteral(".uno:AlignDown")};
        bool ok=false;
        const auto label=QInputDialog::getItem(tab,QObject::tr("Căn đối tượng"),
            QObject::tr("Chọn kiểu căn theo vùng đối tượng đã chọn. Với một đối tượng, engine có thể căn theo slide."),
            labels,0,false,&ok);
        if(!ok||!target.verify(true))return true;
        const int index=labels.indexOf(label);
        if(index<0)return true;
        const QString command=commands.at(index);
        const auto state=tab->states().value(command);
        if(state!=QLatin1String("enabled")&&state!=QLatin1String("true")&&state!=QLatin1String("false")){
            QMessageBox::information(tab,QObject::tr("Căn đối tượng"),QObject::tr("Engine chưa xác nhận lệnh khả dụng cho vùng chọn này."));
            return true;
        }
        d->command(command.toStdString());d->waitInputIdle();tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.layerUp") || id == QLatin1String("ppt.layerDown") || id == QLatin1String("ppt.group") || id == QLatin1String("ppt.ungroup")) {
        if(d->readOnly())return true;
        const PptEditTarget target(tab);
        const QString command=id==QLatin1String("ppt.group")?QStringLiteral(".uno:FormatGroup"):
            id==QLatin1String("ppt.ungroup")?QStringLiteral(".uno:FormatUngroup"):
            id==QLatin1String("ppt.layerUp")?QStringLiteral(".uno:Forward"):QStringLiteral(".uno:Backward");
        static const QRegularExpression rect(QStringLiteral("^\\s*-?\\d+\\s*,\\s*-?\\d+\\s*,\\s*[1-9]\\d*\\s*,\\s*[1-9]\\d*(?:\\s*,|\\s*$)"));
        const auto state=tab->states().value(command);
        if(!rect.match(QString::fromStdString(target.graphic)).hasMatch()||
           (state!=QLatin1String("enabled")&&state!=QLatin1String("true")&&state!=QLatin1String("false"))||
           !target.selected.empty()){
            QMessageBox::information(tab,QObject::tr("Sắp xếp đối tượng"),QObject::tr("Chọn đối tượng trên slide (không ở chế độ sửa chữ). Để nhóm, chọn nhiều hình bằng Shift; để bỏ nhóm, chọn nhóm hiện có. Lệnh phải khả dụng."));
            return true;
        }
        if(!target.verify(true))return true;
        d->command(command.toStdString());
        d->waitInputIdle();
        tab->view()->setFocus();
    } else if (id == QLatin1String("ppt.textbox")) {
        armShape(tab, ".uno:Text");
    } else if (id == QLatin1String("ppt.rect")) {
        armShape(tab, ".uno:BasicShapes.rectangle");
    } else if (id == QLatin1String("ppt.image")) {
        if (d->readOnly()) return true;
        tab->setPptNotesMode(false);
        const PptEditTarget target(tab);
        const QString f = QFileDialog::getOpenFileName(tab, QObject::tr("Chèn ảnh"), QString(),
                                                       QObject::tr("Ảnh (*.png *.jpg *.jpeg *.bmp *.gif)"));
        if (f.isEmpty() || !target.verify(false)) return true;
        const QFileInfo image(f);
        if (!image.isFile() || !image.isReadable()) {
            QMessageBox::warning(tab, QObject::tr("Chèn ảnh"), QObject::tr("Tệp ảnh không còn đọc được."));
            return true;
        }
        d->command(".uno:InsertGraphic", dung::Document::stringArg("FileName", ss(QUrl::fromLocalFile(f).toString())));
        if (target.tab && target.tab->view()) target.tab->view()->setFocus();
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
    } else if(id==QLatin1String("ppt.clearAnimationCopy")){
        const QString source=QFileDialog::getOpenFileName(tab,QObject::tr("Chọn PPTX đã lưu"),d->isNew()?QString():fromPath(d->originalPath()),QObject::tr("PowerPoint (*.pptx)"));
        if(source.isEmpty())return true;
        QString error;const auto slides=ppttransition::slideEntries(source,&error);
        if(slides.isEmpty()){QMessageBox::warning(tab,QObject::tr("Xóa hoạt ảnh"),error);return true;}
        bool ok=false;
        const int page=QInputDialog::getInt(tab,QObject::tr("Xóa hoạt ảnh"),QObject::tr("Slide cần xóa toàn bộ timing:"),1,1,int(slides.size()),1,&ok);
        if(!ok)return true;
        if(QMessageBox::question(tab,QObject::tr("Xóa toàn bộ hoạt ảnh"),
            QObject::tr("Slide %1: xóa toàn bộ timing, kể cả hiệu ứng phức tạp và trigger. Nội dung slide và chuyển cảnh được giữ; chỉ tạo bản PPTX mới. Tiếp tục?").arg(page),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return true;
        const QFileInfo src(source);
        const QString dest=QFileDialog::getSaveFileName(tab,QObject::tr("Lưu PPTX mới"),src.absolutePath()+QLatin1Char('/')+src.completeBaseName()+QStringLiteral(" - xoa hoat anh.pptx"),QObject::tr("PowerPoint (*.pptx)"));
        if(dest.isEmpty())return true;
        const auto result=ppttransition::createClearAnimationCopy(source,dest,page);
        if(!result.ok)QMessageBox::warning(tab,QObject::tr("Xóa hoạt ảnh"),result.error);
        else QMessageBox::information(tab,QObject::tr("Xóa hoạt ảnh"),QObject::tr("Đã tạo bản sao không có timing trên slide %1. Có thể dùng Thêm hoạt ảnh để thiết lập lại.").arg(page));
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
        auto* effect = new QComboBox; effect->addItems({QObject::tr("Xuất hiện ngay"), QObject::tr("Mờ dần vào"), QObject::tr("Biến mất"), QObject::tr("Mờ dần ra")}); effect->setCurrentIndex(1);
        auto* duration = new QDoubleSpinBox; duration->setRange(0.1,10.0); duration->setDecimals(1); duration->setSingleStep(0.1); duration->setValue(0.5); duration->setSuffix(QObject::tr(" giây"));
        auto* trigger=new QComboBox;
        trigger->addItems({QObject::tr("Khi nhấp chuột"),QObject::tr("Cùng hiệu ứng trước"),QObject::tr("Sau hiệu ứng trước")});
        auto* delay = new QDoubleSpinBox; delay->setRange(0,30); delay->setDecimals(1); delay->setSingleStep(0.1); delay->setSuffix(QObject::tr(" giây"));
        QObject::connect(effect, &QComboBox::currentIndexChanged, duration, [duration](int n) { duration->setEnabled(n==1 || n==3); });
        form->addRow(QObject::tr("Đối tượng"),shape); form->addRow(QObject::tr("Hiệu ứng"),effect);
        form->addRow(QObject::tr("Bắt đầu"),trigger); form->addRow(QObject::tr("Thời lượng"),duration); form->addRow(QObject::tr("Độ trễ"),delay); root->addLayout(form);
        auto* buttons = new QHBoxLayout;
        auto* add = new QPushButton(QObject::tr("+ Thêm"), &dialog); add->setObjectName(QStringLiteral("AnimationPrimary"));
        auto* edit = new QPushButton(QObject::tr("Cập nhật dòng"), &dialog);
        auto* quick = new QPushButton(QObject::tr("Mờ dần lần lượt tất cả"), &dialog);
        buttons->addWidget(add); buttons->addWidget(edit); buttons->addWidget(quick); root->addLayout(buttons);
        auto* list = new AnimationOrderList(&dialog); list->setSelectionMode(QAbstractItemView::SingleSelection);
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
        auto* note = new QLabel(QObject::tr("Kéo thả để đổi thứ tự. Dòng đầu phải là khi nhấp chuột. Các dòng sau có thể cùng/sau hiệu ứng trước. Thời gian tính từ lần nhấp của nhóm. Chỉ dùng slide chưa có timing; đây là timeline dự kiến, chưa phát thử trực tiếp."), &dialog); note->setWordWrap(true); root->addWidget(note);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); bb->button(QDialogButtonBox::Ok)->setText(QObject::tr("Xuất PPTX mới…")); root->addWidget(bb);
        QList<ppttransition::AnimationOptions> effects;
        auto read = [&] {
            ppttransition::AnimationOptions o; o.shapeId=shape->currentData().toUInt(); o.fade=effect->currentIndex()%2==1; o.exit=effect->currentIndex()>=2;
            o.durationMs=qRound(duration->value()*1000); o.delayMs=qRound(delay->value()*1000); o.trigger=ppttransition::AnimationTrigger(trigger->currentIndex()); return o;
        };
        auto updateTimeline=[&] {
            QList<ppttransition::AnimationTiming> plan;
            const auto result=ppttransition::planAnimation(effects,&plan);
            for(int i=0;i<effects.size() && i<list->count();++i){
                const auto& o=effects[i];const int ix=shape->findData(o.shapeId);
                const QString time=result.ok?QObject::tr("Nhấp %1 · %2–%3 s").arg(plan[i].click)
                    .arg(plan[i].startMs/1000.0,0,'f',2).arg(plan[i].endMs/1000.0,0,'f',2):QObject::tr("Chưa hợp lệ");
                list->item(i)->setText(QObject::tr("%1. %2 · %3 · %4 · %5")
                    .arg(i+1).arg(shape->itemText(ix)).arg(effect->itemText((o.exit?2:0)+(o.fade?1:0)))
                    .arg(trigger->itemText(int(o.trigger)),time));
            }
            bb->button(QDialogButtonBox::Ok)->setEnabled(result.ok);
            note->setText(effects.isEmpty()?QObject::tr("Thêm hiệu ứng; dòng đầu phải là khi nhấp chuột."):
                result.ok?QObject::tr("Timeline dự kiến theo từng lần nhấp. Mỗi đối tượng có tối đa một hiệu ứng vào rồi một hiệu ứng ra, không chồng thời gian. Chưa phát thử trực tiếp; kiểm tra trong PowerPoint sau khi xuất."):result.error);
        };
        auto refresh = [&](int row) {
            { QSignalBlocker block(list);list->clear();
              for(int i=0;i<effects.size();++i){auto* item=new QListWidgetItem(list);item->setData(Qt::UserRole,i);}
              updateTimeline();
            }
            list->setCurrentRow(row);
            edit->setEnabled(row>=0);remove->setEnabled(row>=0);
            up->setEnabled(row>0);down->setEnabled(row>=0&&row+1<effects.size());
        };
        auto syncOrder=[&] {
            if(list->count()!=effects.size())return false;
            QList<ppttransition::AnimationOptions> ordered;QSet<int> seen;
            for(int i=0;i<list->count();++i){
                bool ok=false;const int index=list->item(i)->data(Qt::UserRole).toInt(&ok);
                if(!ok || index<0 || index>=effects.size() || seen.contains(index))return false;
                ordered.append(effects[index]);seen.insert(index);
            }
            effects=ordered;return true;
        };
        list->orderChanged=[&] {
            const int selected=list->currentRow();
            if(!syncOrder()){
                refresh(qBound(-1,selected,int(effects.size())-1));
                note->setText(QObject::tr("Không xác minh được thứ tự kéo thả. Đã giữ thứ tự trước đó."));
                return;
            }
            refresh(selected);
        };
        auto duplicate = [&](const ppttransition::AnimationOptions& candidate, int except) {
            for (int i=0;i<effects.size();++i)
                if (i!=except && effects[i].shapeId==candidate.shapeId && effects[i].exit==candidate.exit) return true;
            return false;
        };
        QObject::connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row) {
            edit->setEnabled(row>=0); remove->setEnabled(row>=0); up->setEnabled(row>0); down->setEnabled(row>=0 && row+1<effects.size());
            if (row<0 || row>=effects.size()) return;
            const auto& o=effects[row]; shape->setCurrentIndex(shape->findData(o.shapeId)); effect->setCurrentIndex((o.exit?2:0)+(o.fade?1:0));
            duration->setValue(o.durationMs/1000.0); delay->setValue(o.delayMs/1000.0); trigger->setCurrentIndex(int(o.trigger));
        });
        QObject::connect(add,&QPushButton::clicked,&dialog,[&] {
            const auto o=read();
            if (effects.size()>=100 || duplicate(o,-1)) { QMessageBox::information(&dialog,QObject::tr("Hoạt ảnh"),QObject::tr("Đối tượng đã có hiệu ứng cùng loại vào/ra hoặc danh sách đủ 100 dòng.")); return; }
            effects.append(o); refresh(int(effects.size())-1);
        });
        QObject::connect(edit,&QPushButton::clicked,&dialog,[&] {
            const int row=list->currentRow(); if(row<0) return; const auto o=read();
            if(duplicate(o,row)) { QMessageBox::information(&dialog,QObject::tr("Hoạt ảnh"),QObject::tr("Đối tượng đã có hiệu ứng cùng loại vào/ra ở dòng khác.")); return; }
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
        QObject::connect(bb,&QDialogButtonBox::accepted,&dialog,[&]{
            if(!syncOrder()){QMessageBox::warning(&dialog,QObject::tr("Hoạt ảnh"),QObject::tr("Thứ tự chưa hợp lệ; chưa xuất."));return;}
            QList<ppttransition::AnimationTiming> plan;
            const auto valid=ppttransition::planAnimation(effects,&plan);
            if(!valid.ok){refresh(list->currentRow());note->setText(valid.error);return;}
            dialog.accept();
        }); QObject::connect(bb,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
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
        auto* rangeEnd=new QSpinBox;rangeEnd->setRange(1,slides.size());rangeEnd->setValue(page->value());
        QObject::connect(page,qOverload<int>(&QSpinBox::valueChanged),&dialog,[rangeEnd](int first){rangeEnd->setMinimum(first);if(rangeEnd->value()<first)rangeEnd->setValue(first);});
        auto* all=new QCheckBox(QObject::tr("Áp dụng cho tất cả slide"));
        QObject::connect(all,&QCheckBox::toggled,&dialog,[page,rangeEnd,slides](bool enabled){
            if(enabled){page->setValue(1);rangeEnd->setValue(int(slides.size()));}
            page->setEnabled(!enabled);rangeEnd->setEnabled(!enabled);
        });
        auto* effect = new QComboBox;
        effect->addItems({QObject::tr("Không hiệu ứng"), QObject::tr("Mờ dần"), QObject::tr("Đẩy sang trái"), QObject::tr("Quét sang trái"), QObject::tr("Hòa tan")});
        effect->setCurrentIndex(1);
        auto* speed = new QComboBox; speed->addItems({QObject::tr("Chậm"), QObject::tr("Vừa"), QObject::tr("Nhanh")}); speed->setCurrentIndex(1);
        auto* click = new QCheckBox(QObject::tr("Chuyển khi bấm chuột")); click->setChecked(true);
        auto* seconds = new QSpinBox; seconds->setRange(0, 600); seconds->setSpecialValueText(QObject::tr("Tắt tự chuyển"));
        form->addRow(QObject::tr("Từ slide"),page);form->addRow(QObject::tr("Đến slide"),rangeEnd);form->addRow(all);
        rangeEnd->setMinimum(page->value());
        form->addRow(QObject::tr("Hiệu ứng"), effect);
        form->addRow(QObject::tr("Tốc độ"), speed); form->addRow(click); form->addRow(QObject::tr("Tự chuyển sau (giây)"), seconds); root->addLayout(form);
        auto* note = new QLabel(QObject::tr("Chuyển cảnh cơ bản được ghi vào PPTX; mở bản xuất bằng PowerPoint để kiểm tra. Tối đa 100 slide mỗi lần; chuyển cảnh phức tạp chưa hỗ trợ sẽ bị từ chối trước khi xuất bản sao.")); note->setWordWrap(true); root->addWidget(note);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); root->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return true;
        const QFileInfo src(source);
        const QString dest = QFileDialog::getSaveFileName(tab, QObject::tr("Lưu bản PPTX mới"), src.absolutePath() + QLatin1Char('/') + src.completeBaseName() + QStringLiteral(" - chuyen canh.pptx"), QObject::tr("PowerPoint (*.pptx)"));
        if (dest.isEmpty()) return true;
        ppttransition::Options o; o.effect = ppttransition::Effect(effect->currentIndex()); o.speed = speed->currentIndex(); o.onClick = click->isChecked(); o.autoAdvanceMs = seconds->value()*1000;
        const auto result = ppttransition::createRangeCopy(source,dest,page->value(),rangeEnd->value(),o);
        if (!result.ok) QMessageBox::warning(tab, QObject::tr("Chuyển cảnh"), result.error);
        else QMessageBox::information(tab, QObject::tr("Chuyển cảnh"), QObject::tr("Đã lưu PPTX mới và kiểm cấu trúc gói:\n%1\nHãy thử trình chiếu trong PowerPoint; nguồn không bị sửa.").arg(dest));
    }
    return true;
}
