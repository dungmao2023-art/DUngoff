// SPDX-License-Identifier: MPL-2.0
#include "pdf/PdfToolsDialog.h"
#include "pdf/PdfOps.h"
#include <QApplication>
#include <QPdfWriter>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFileInfo>
#include <QDir>
#include <iostream>

int main(int argc,char** argv){
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    QTemporaryDir temp;
    if(!temp.isValid())return 1;
    const auto source=temp.filePath(QStringLiteral("reader-test.pdf"));
    {
        QPdfWriter writer(source);
        writer.setResolution(72);
        QPainter p(&writer);
        if(!p.isActive())return 1;
        p.drawText(40,80,QStringLiteral("PAGE ONE"));
        if(!writer.newPage())return 1;
        p.drawText(40,80,QStringLiteral("PAGE TWO"));
        p.end();
    }
    const auto hash=dungpdf::fingerprint(source);
    int failures=0;bool ran=false;
    auto check=[&](bool ok,const char* label){
        std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;
        if(!ok)++failures;
    };
    QTimer::singleShot(0,&app,[&]{
        auto* dlg=qobject_cast<PdfToolsDialog*>(QApplication::activeModalWidget());
        if(!dlg){check(false,"reader dialog open");QApplication::exit(1);return;}
        ran=true;
        auto* tools=dlg->findChild<QListWidget*>("pdfToolList");
        auto* file=dlg->findChild<QLineEdit*>("pdfReaderSource");
        auto* row=dlg->findChild<QWidget*>("pdfReaderSourceRow");
        auto* search=dlg->findChild<QWidget*>("pdfSearchPanel");
        auto* query=dlg->findChild<QLineEdit*>("pdfSearchQuery");
        auto* searchToggle=dlg->findChild<QPushButton*>("pdfSearchToggle");
        auto* textToggle=dlg->findChild<QPushButton*>("pdfTextToggle");
        auto* extracted=dlg->findChild<QPlainTextEdit*>("pdfExtractedText");
        auto* pageList=dlg->findChild<QListWidget*>("pdfReaderPageList");
        auto* pageListToggle=dlg->findChild<QPushButton*>("pdfPageListToggle");
        auto* next=dlg->findChild<QPushButton*>("pdfNextPage");
        auto* page=dlg->findChild<QSpinBox*>("pdfPageNumber");
        auto* start=dlg->findChild<QPushButton*>("pdfSessionStart");
        auto* cancel=dlg->findChild<QPushButton*>("pdfSessionCancel");
        if(!tools||!file||!row||!search||!query||!searchToggle||!textToggle||!extracted||!next||!page||!start||!cancel||!pageList||!pageListToggle){
            check(false,"reader controls present");dlg->reject();return;
        }
        check(file->text()==source&&page->value()==1&&page->maximum()==2&&next->isEnabled(),"automatically open first page");
        check(search->isHidden()&&extracted->isHidden()&&pageList->isHidden(),"compact reading layout by default");
        pageListToggle->click();
        check(!pageList->isHidden()&&pageList->count()==2,"show page list on demand");
        pageListToggle->click();
        check(pageList->isHidden(),"hide page list without losing document");
        next->click();
        check(page->value()==2&&extracted->toPlainText().contains("PAGE TWO"),"navigate to real second page");
        searchToggle->click();query->setText("PAGE");
        check(!search->isHidden()&&page->value()==2&&extracted->toPlainText().contains("PAGE TWO"),"typing search does not clear reading page");
        query->setText("NO SUCH TEXT 1937");
        QMetaObject::invokeMethod(query,"returnPressed",Qt::DirectConnection);
        check(page->value()==2&&extracted->toPlainText().contains("PAGE TWO"),"search miss keeps reading page");
        query->setText("PAGE");
        QMetaObject::invokeMethod(query,"returnPressed",Qt::DirectConnection);
        check(!pageList->isHidden()&&pageList->count()==2,"search hits reveal results list");
        query->setText("PAGE TWO");
        check(extracted->toPlainText().contains("PAGE ONE"),"changing query keeps current reading page");
        pageListToggle->click();
        searchToggle->click();textToggle->click();
        check(search->isHidden()&&!extracted->isHidden(),"toggle extracted text panel");
        textToggle->click();
        start->click();
        const auto working=file->text();
        check(working!=source&&dungpdf::fingerprint(working)==hash&&!row->isEnabled()&&!tools->isEnabled()&&!cancel->isHidden(),
              "session snapshot and lock other tools");
        cancel->click();
        check(file->text()==source&&row->isEnabled()&&tools->isEnabled()&&cancel->isHidden()&&dungpdf::fingerprint(source)==hash,
              "cancel returns to original without changing it");
        // Drive a real note through the canvas, not a private test-only API.
        auto* canvas=dlg->findChild<QWidget*>("pdfPageCanvas");
        auto* note=dlg->findChild<QPushButton*>("pdfAddNote");
        auto* undo=dlg->findChild<QPushButton*>("pdfSessionUndo");
        auto* redo=dlg->findChild<QPushButton*>("pdfSessionRedo");
        auto* save=dlg->findChild<QPushButton*>("pdfSessionSave");
        if(!canvas||!note||!undo||!redo||!save||!note->isEnabled()){
            check(false,"direct editing controls available");dlg->reject();return;
        }
        auto pressShortcut=[&](QKeySequence::StandardKey key){
            const auto bindings=QKeySequence::keyBindings(key);
            if(bindings.isEmpty()||bindings.first().isEmpty()){check(false,"standard key binding available");return;}
            const auto combo=bindings.first()[0];
            QKeyEvent event(QEvent::KeyPress,int(combo.key()),combo.keyboardModifiers());
            QApplication::sendEvent(canvas,&event);
        };
        auto pressKey=[&](int key,Qt::KeyboardModifiers mods=Qt::NoModifier){
            QKeyEvent event(QEvent::KeyPress,key,mods);QApplication::sendEvent(canvas,&event);
        };
        note->click();
        check(note->isChecked()&&canvas->hasFocus(),"placement mode focuses page");
        pressKey(Qt::Key_Escape);
        check(!note->isChecked()&&dlg->isVisible()&&dungpdf::fingerprint(source)==hash,"Escape cancels placement without closing or modifying PDF");
        const int beforePage=page->value();
        pressKey(beforePage==1?Qt::Key_PageDown:Qt::Key_PageUp,Qt::ControlModifier);
        check(page->value()==(beforePage==1?2:1),"keyboard navigates document pages");
        pressKey(beforePage==1?Qt::Key_PageUp:Qt::Key_PageDown,Qt::ControlModifier);
        check(page->value()==beforePage,"keyboard returns to previous page");
        pressShortcut(QKeySequence::Find);
        check(!search->isHidden()&&query->hasFocus(),"find shortcut opens and focuses search");
        searchToggle->click();
        start->click();
        const auto base=file->text();
        const auto final=temp.filePath(QStringLiteral("reader-final.pdf"));
        enum class Expected{None,Note,CloseCancel,Save};
        Expected expected=Expected::None;
        QTimer modal;
        modal.setInterval(25);
        QObject::connect(&modal,&QTimer::timeout,dlg,[&]{
            auto* active=QApplication::activeModalWidget();
            if(!active||active==dlg)return;
            if(auto* input=qobject_cast<QInputDialog*>(active);input&&expected==Expected::Note){
                expected=Expected::None;input->setTextValue(QStringLiteral("Ghi chú kiểm thử"));input->accept();return;
            }
            if(auto* box=qobject_cast<QMessageBox*>(active);box&&expected==Expected::CloseCancel){
                expected=Expected::None;auto* button=box->button(QMessageBox::Cancel);
                if(button){button->click();return;}
            }
            if(auto* dialog=qobject_cast<QFileDialog*>(active);dialog&&expected==Expected::Save){
                expected=Expected::None;
                const QFileInfo target(final);
                dialog->setDirectory(target.absolutePath());
                dialog->selectFile(target.fileName());
                // A visible non-native QFileDialog can retain the previous
                // filename in its editor after selectFile; drive the actual
                // input control just as a user entering the new name would.
                if(auto* editor=dialog->findChild<QLineEdit*>("fileNameEdit"))
                    editor->setText(target.fileName());
                const auto chosen=dialog->selectedFiles();
                const bool exact=chosen.size()==1&&QDir::cleanPath(chosen.first())==QDir::cleanPath(final);
                check(exact,"save dialog resolves requested final filename");
                if(!exact){
                    std::cerr<<"Selected files: "<<chosen.join(" | ").toStdString()
                             <<" expected: "<<final.toStdString()<<std::endl;
                    dialog->reject();return;
                }
                QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);return;
            }
            // Fail instead of hanging CI on any unexpected dialog.
            check(false,"unexpected modal while exercising PDF edit");
            if(auto* dialog=qobject_cast<QDialog*>(active))dialog->reject();
        });
        modal.start();
        note->click();expected=Expected::Note;
        const QPointF point=canvas->rect().center();
        QMouseEvent click(QEvent::MouseButtonPress,point,canvas->mapToGlobal(point.toPoint()),
                          Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas,&click);
        const auto revised=file->text();
        const auto revisedHash=dungpdf::fingerprint(revised);
        check(expected==Expected::None&&revised!=base&&revisedHash.size()==32&&revisedHash!=hash&&undo->isEnabled()&&save->isEnabled(),
              "place real note and stage modified PDF");
        pressShortcut(QKeySequence::Undo);
        check(file->text()==base&&dungpdf::fingerprint(base)==hash&&redo->isEnabled()&&!save->isEnabled(),"undo real note");
        pressShortcut(QKeySequence::Redo);
        check(file->text()==revised&&dungpdf::fingerprint(revised)==revisedHash&&save->isEnabled(),"redo real note");
        expected=Expected::CloseCancel;
        dlg->reject();
        check(expected==Expected::None&&dlg->isVisible()&&file->text()==revised,"cancel close preserves unsaved changes");
        expected=Expected::Save;pressShortcut(QKeySequence::Save);
        check(expected==Expected::None,"save dialog completed");
        check(QFileInfo::exists(final),"final PDF exists at chosen path");
        check(dungpdf::fingerprint(final)==revisedHash,"final PDF bytes match edited revision");
        check(dungpdf::fingerprint(source)==hash,"original PDF bytes unchanged");
        check(QDir::cleanPath(file->text())==QDir::cleanPath(final),"reader switches to exported PDF");
        check(tools->isEnabled()&&cancel->isHidden(),"session exits and tools unlock");
        if(QDir::cleanPath(file->text())!=QDir::cleanPath(final))
            std::cerr<<"Reader source: "<<file->text().toStdString()<<" expected: "<<final.toStdString()<<std::endl;
        modal.stop();
        // If the last step failed, reject an unsaved-close prompt via Discard
        // so the test reports failure rather than blocking indefinitely.
        QTimer closeGuard;
        QObject::connect(&closeGuard,&QTimer::timeout,dlg,[]{
            if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
                if(auto* b=box->button(QMessageBox::Discard))b->click();else box->reject();
            }
        });
        closeGuard.start(25);dlg->reject();closeGuard.stop();
    });
    PdfToolsDialog::run(nullptr,PdfToolsDialog::Search,source);
    return ran&&failures==0?0:1;
}
