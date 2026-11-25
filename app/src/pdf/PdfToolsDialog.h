// SPDX-License-Identifier: MPL-2.0
// Hộp thoại "Công cụ PDF": gộp, tách, xoay, nén, thông tin. Gọi PdfOps (qpdf), tuân quy tắc
// an toàn dữ liệu: không bao giờ ghi đè tệp gốc, luôn lưu ra tệp mới (hoặc thay có sao lưu).
#pragma once
#include "PdfOps.h"

#include <QDialog>
#include <QHash>
#include <functional>

class QTabWidget;
class QListWidget;
class QLineEdit;
class QLabel;
class QComboBox;
class QRadioButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;

class PdfToolsDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { Merge = 0, Split = 1, Rotate = 2, Compress = 3, Info = 4, Number = 5, Stamp = 6, Images = 7, Blank = 8, Reorder = 9 };
    // Mở hộp thoại (chặn cho tới khi đóng). `initialPath` là PDF đang mở (có thể rỗng).
    static void run(QWidget* parent, Tab tab, const QString& initialPath);

private:
    explicit PdfToolsDialog(QWidget* parent);

    QWidget* buildMerge();
    QWidget* buildSplit();
    QWidget* buildRotate();
    QWidget* buildCompress();
    QWidget* buildInfo();
    QWidget* buildNumber();
    QWidget* buildStamp();
    QWidget* buildImages();
    QWidget* buildBlank();
    QWidget* buildReorder();
    QWidget* fileRow(QLineEdit** edit, const std::function<void()>& onChosen);

    QString pickPdf();
    dungpdf::Input inputFor(const QString& path) const;
    // Chạy `op` ở luồng nền; nếu tệp cần mật khẩu thì hỏi rồi chạy lại (tối đa 3 lần).
    dungpdf::Result runWithPasswords(const QStringList& paths, const std::function<dungpdf::Result()>& op);
    bool askPassword(const QString& path);
    void showResult(const dungpdf::Result& r, const QString& okTitle);
    void refreshPageCount(const QString& path, QLabel* label);

    QTabWidget* m_tabs = nullptr;
    QHash<QString, QString> m_passwords;  // đường dẫn -> mật khẩu (chỉ trong phiên hộp thoại)

    QListWidget* m_mergeList = nullptr;
    QLineEdit* m_splitFile = nullptr;
    QLabel* m_splitInfo = nullptr;
    QRadioButton* m_splitKeep = nullptr;
    QLineEdit* m_splitPages = nullptr;
    QLineEdit* m_rotFile = nullptr;
    QLabel* m_rotInfo = nullptr;
    QLineEdit* m_rotPages = nullptr;
    QComboBox* m_rotAngle = nullptr;
    QLineEdit* m_cmpFile = nullptr;
    QLineEdit* m_infoFile = nullptr;
    QLabel* m_infoText = nullptr;

    QLineEdit* m_numFile = nullptr;
    QLabel* m_numInfo = nullptr;
    QLineEdit* m_numFormat = nullptr;
    QComboBox* m_numPos = nullptr;
    QSpinBox* m_numStart = nullptr;
    QDoubleSpinBox* m_numSize = nullptr;
    QLineEdit* m_numPages = nullptr;
    QLineEdit* m_stampFile = nullptr;
    QLabel* m_stampInfo = nullptr;
    QLineEdit* m_stampText = nullptr;
    QLineEdit* m_stampImage = nullptr;
    QComboBox* m_stampPos = nullptr;
    QDoubleSpinBox* m_stampSize = nullptr;
    QDoubleSpinBox* m_stampOpacity = nullptr;
    QDoubleSpinBox* m_stampWidth = nullptr;
    QLineEdit* m_stampPages = nullptr;
    QListWidget* m_imgList = nullptr;
    QCheckBox* m_imgA4 = nullptr;
    QLineEdit* m_blankFile = nullptr;
    QLabel* m_blankInfo = nullptr;
    QLineEdit* m_orderFile = nullptr;
    QLabel* m_orderInfo = nullptr;
    QLineEdit* m_orderSpec = nullptr;
};
