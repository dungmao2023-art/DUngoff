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
#include <iostream>

int main(int argc,char** argv){
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
        auto* next=dlg->findChild<QPushButton*>("pdfNextPage");
        auto* page=dlg->findChild<QSpinBox*>("pdfPageNumber");
        auto* start=dlg->findChild<QPushButton*>("pdfSessionStart");
        auto* cancel=dlg->findChild<QPushButton*>("pdfSessionCancel");
        if(!tools||!file||!row||!search||!query||!searchToggle||!textToggle||!extracted||!next||!page||!start||!cancel){
            check(false,"reader controls present");dlg->reject();return;
        }
        check(file->text()==source&&page->value()==1&&page->maximum()==2&&next->isEnabled(),"automatically open first page");
        check(search->isHidden()&&extracted->isHidden(),"compact reading layout by default");
        next->click();
        check(page->value()==2&&extracted->toPlainText().contains("PAGE TWO"),"navigate to real second page");
        searchToggle->click();query->setText("PAGE");
        check(!search->isHidden()&&page->value()==2&&extracted->toPlainText().contains("PAGE TWO"),"typing search does not clear reading page");
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
        dlg->reject();
    });
    PdfToolsDialog::run(nullptr,PdfToolsDialog::Search,source);
    return ran&&failures==0?0:1;
}
