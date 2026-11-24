#include "PrintDialog.h"
#include "PrintLogic.h"
#include "PrintRender.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPageSize>
#include <QPixmap>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

void PrintDialog::run(QWidget* parent, const QString& pdfPath, int displayPages, const QString& title) {
    if (!printrender::available()) {
        // Không có bộ vẽ PDF: mở bằng trình xem PDF mặc định để người dùng in từ đó (không giả vờ có xem trước).
        QMessageBox::information(parent, QObject::tr("In"),
            QObject::tr("Bản dựng này chưa có xem trước in tích hợp. Dũng Office sẽ mở bản in (PDF) bằng trình xem PDF của máy để bạn in từ đó."));
        QDesktopServices::openUrl(QUrl::fromLocalFile(pdfPath));
        return;
    }
    const auto info = printrender::pdfInfo(pdfPath);
    if (!info.ok) {
        QMessageBox::warning(parent, QObject::tr("In"), QObject::tr("Không chuẩn bị được bản in: %1").arg(info.error));
        return;
    }
    PrintDialog dlg(parent, pdfPath, displayPages, title);
    dlg.exec();
}

PrintDialog::PrintDialog(QWidget* parent, const QString& pdfPath, int displayPages, const QString& title)
    : QDialog(parent), m_pdf(pdfPath) {
    setWindowTitle(tr("In — %1").arg(title));
    resize(980, 700);
    m_total = printrender::pdfInfo(pdfPath).pages;

    m_scroll = new QScrollArea(this);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:#e9eef7;border:1px solid #c9d4ea;}"));
    m_img = new QLabel;
    m_img->setAlignment(Qt::AlignCenter);
    m_scroll->setWidget(m_img);
    m_scroll->setWidgetResizable(true);

    auto* prev = new QPushButton(tr("◀ Trước"));
    auto* next = new QPushButton(tr("Sau ▶"));
    m_pageLbl = new QLabel;
    m_pageLbl->setAlignment(Qt::AlignCenter);
    auto* nav = new QHBoxLayout;
    nav->addWidget(prev);
    nav->addWidget(m_pageLbl, 1);
    nav->addWidget(next);

    auto* left = new QVBoxLayout;
    left->addWidget(m_scroll, 1);
    left->addLayout(nav);

    m_printer = new QComboBox;
    const QStringList names = QPrinterInfo::availablePrinterNames();
    const QString def = QPrinterInfo::defaultPrinterName();
    for (const QString& n : names) m_printer->addItem(n);
    if (names.isEmpty()) m_printer->addItem(tr("(Không thấy máy in nào)"));
    const int di = m_printer->findText(def);
    if (di >= 0) m_printer->setCurrentIndex(di);

    m_paper = new QComboBox;
    m_paper->addItem(tr("Mặc định của máy in"), -1);
    m_paper->addItem(QStringLiteral("A4"), int(QPageSize::A4));
    m_paper->addItem(QStringLiteral("A3"), int(QPageSize::A3));
    m_paper->addItem(QStringLiteral("A5"), int(QPageSize::A5));
    m_paper->addItem(QStringLiteral("Letter"), int(QPageSize::Letter));
    m_paper->addItem(QStringLiteral("Legal"), int(QPageSize::Legal));
    m_orient = new QComboBox;
    m_orient->addItems({tr("Tự động theo từng trang"), tr("Luôn dọc"), tr("Luôn ngang")});
    m_duplex = new QComboBox;
    m_duplex->addItems({tr("Theo máy in"), tr("In một mặt"), tr("In hai mặt (lật cạnh dài)"), tr("In hai mặt (lật cạnh ngắn)")});
    m_copies = new QSpinBox;
    m_copies->setRange(1, 99);
    m_collate = new QCheckBox(tr("Sắp theo bộ (1,2,3 · 1,2,3)"));
    m_collate->setChecked(true);
    m_gray = new QCheckBox(tr("In đen trắng"));
    m_range = new QLineEdit;
    m_range->setPlaceholderText(tr("Tất cả (hoặc 1-3, 5, 8-)"));

    auto* form = new QFormLayout;
    form->addRow(tr("Máy in"), m_printer);
    form->addRow(tr("Khoảng trang"), m_range);
    form->addRow(tr("Số bản"), m_copies);
    form->addRow(QString(), m_collate);
    form->addRow(tr("Khổ giấy"), m_paper);
    form->addRow(tr("Hướng giấy"), m_orient);
    form->addRow(tr("Hai mặt"), m_duplex);
    form->addRow(QString(), m_gray);

    m_warn = new QLabel;
    m_warn->setWordWrap(true);
    m_warn->setStyleSheet(QStringLiteral("color:#9a5b00;background:#fff4d6;border:1px solid #f0d28a;padding:6px;"));
    m_warn->setVisible(false);
    const QString w = printlogic::mismatchWarning(m_total, displayPages);
    if (!w.isEmpty()) { m_warn->setText(w); m_warn->setVisible(true); }

    auto* info = new QLabel(tr("Bản xem trước là đúng tệp sẽ được gửi tới máy in (%1 trang).").arg(m_total));
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color:#4c5d7a;"));

    m_print = new QPushButton(tr("In"));
    m_print->setDefault(true);
    m_print->setStyleSheet(QStringLiteral("QPushButton{background:#2f6fe4;color:white;padding:6px 18px;border-radius:4px;}"));
    auto* close = new QPushButton(tr("Đóng"));
    auto* btns = new QHBoxLayout;
    btns->addStretch();
    btns->addWidget(close);
    btns->addWidget(m_print);

    auto* right = new QVBoxLayout;
    right->addLayout(form);
    right->addWidget(m_warn);
    right->addWidget(info);
    right->addStretch();
    right->addLayout(btns);

    auto* root = new QHBoxLayout(this);
    root->addLayout(left, 1);
    auto* rw = new QWidget;
    rw->setLayout(right);
    rw->setFixedWidth(330);
    root->addWidget(rw);

    connect(prev, &QPushButton::clicked, this, [this] { showPage(m_cur - 1); });
    connect(next, &QPushButton::clicked, this, [this] { showPage(m_cur + 1); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_print, &QPushButton::clicked, this, &PrintDialog::doPrint);
    showPage(1);
}

void PrintDialog::resizeEvent(QResizeEvent* e) {
    QDialog::resizeEvent(e);
    if (m_img && m_total > 0) showPage(m_cur);
}

void PrintDialog::showPage(int page1) {
    if (page1 < 1 || page1 > m_total) return;
    m_cur = page1;
    const QSize box = m_scroll->viewport()->size() - QSize(16, 16);
    QString err;
    const QImage img = printrender::renderPage(m_pdf, page1, box.expandedTo(QSize(200, 200)), &err);
    if (img.isNull()) m_img->setText(tr("Không vẽ được trang: %1").arg(err));
    else m_img->setPixmap(QPixmap::fromImage(img));
    m_pageLbl->setText(tr("Trang %1 / %2").arg(page1).arg(m_total));
}

void PrintDialog::doPrint() {
    const auto rr = printlogic::parseRanges(m_range->text(), m_total);
    if (!rr.ok) { QMessageBox::warning(this, tr("In"), rr.error); return; }
    if (QPrinterInfo::availablePrinterNames().isEmpty()) {
        QMessageBox::warning(this, tr("In"), tr("Không thấy máy in nào trên máy này. Hãy cài hoặc bật máy in rồi thử lại."));
        return;
    }
    const QPrinterInfo pi = QPrinterInfo::printerInfo(m_printer->currentText());
    if (pi.isNull()) { QMessageBox::warning(this, tr("In"), tr("Không mở được máy in đã chọn.")); return; }

    QPrinter printer(pi, QPrinter::HighResolution);
    if (m_paper->currentData().toInt() >= 0)
        printer.setPageSize(QPageSize(QPageSize::PageSizeId(m_paper->currentData().toInt())));
    // Nếu máy in không tự in nhiều bản, ứng dụng tự lặp trang (đúng thứ tự theo bộ/không theo bộ).
    QVector<int> pagesOut = rr.pages;
    if (printer.supportsMultipleCopies()) {
        printer.setCopyCount(m_copies->value());
        printer.setCollateCopies(m_collate->isChecked());
    } else {
        pagesOut = printlogic::expandCopies(rr.pages, m_copies->value(), m_collate->isChecked());
    }
    printer.setColorMode(m_gray->isChecked() ? QPrinter::GrayScale : QPrinter::Color);
    switch (m_duplex->currentIndex()) {
    case 1: printer.setDuplex(QPrinter::DuplexNone); break;
    case 2: printer.setDuplex(QPrinter::DuplexLongSide); break;
    case 3: printer.setDuplex(QPrinter::DuplexShortSide); break;
    default: break;
    }
    const bool autoOrient = m_orient->currentIndex() == 0;
    if (m_orient->currentIndex() == 1) printer.setPageOrientation(QPageLayout::Portrait);
    if (m_orient->currentIndex() == 2) printer.setPageOrientation(QPageLayout::Landscape);

    setCursor(Qt::WaitCursor);
    QString err;
    const bool ok = printrender::printPages(m_pdf, printer, pagesOut, autoOrient, &err);
    unsetCursor();
    if (!ok) { QMessageBox::warning(this, tr("In"), tr("In chưa thành công: %1").arg(err)); return; }
    QMessageBox::information(this, tr("In"),
        tr("Đã gửi %1 trang tới máy in \"%2\".").arg(rr.pages.size()).arg(m_printer->currentText()));
}
