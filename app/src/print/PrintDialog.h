#pragma once
#include <QDialog>
#include <QString>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QScrollArea;

// Xem trước và in. Xem trước vẽ ĐÚNG tệp PDF sẽ gửi tới máy in (cùng một tệp), nên không lệch giữa xem trước và bản in.
class PrintDialog : public QDialog {
    Q_OBJECT
public:
    // displayPages: số trang tài liệu hiển thị trong cửa sổ soạn (<=0: không so sánh được).
    static void run(QWidget* parent, const QString& pdfPath, int displayPages, const QString& title);

private:
    PrintDialog(QWidget* parent, const QString& pdfPath, int displayPages, const QString& title);
    void showPage(int page1);
    void doPrint();
    void resizeEvent(QResizeEvent*) override;

    QString m_pdf;
    int m_total = 0;
    int m_cur = 1;
    QLabel* m_img = nullptr;
    QLabel* m_pageLbl = nullptr;
    QLabel* m_warn = nullptr;
    QScrollArea* m_scroll = nullptr;
    QComboBox* m_printer = nullptr;
    QComboBox* m_paper = nullptr;
    QComboBox* m_orient = nullptr;
    QComboBox* m_duplex = nullptr;
    QSpinBox* m_copies = nullptr;
    QCheckBox* m_collate = nullptr;
    QCheckBox* m_gray = nullptr;
    QLineEdit* m_range = nullptr;
    QPushButton* m_print = nullptr;
};
