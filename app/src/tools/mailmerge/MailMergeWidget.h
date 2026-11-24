// SPDX-License-Identifier: MPL-2.0
// Giao diện "Điền tên hàng loạt". Chưa tự đăng ký vào menu — nơi gọi dùng createMailMergeWidget().
#pragma once
#include <QWidget>
#include <functional>

class MailMergeWidget : public QWidget {
    Q_OBJECT
public:
    explicit MailMergeWidget(QWidget *parent = nullptr);
    ~MailMergeWidget() override;

    // Các móc nối tuỳ chọn (nơi nhúng widget tự cung cấp):
    // đường dẫn .docx của tab Word đang mở ("" nếu không có) — để nút "Dùng tài liệu đang mở" hiện ra.
    void setCurrentDocumentProvider(std::function<QString()> f);
    // đoạn chữ đang bôi chọn trong Word — để nút "Lấy đoạn đang bôi chọn" hiện ra.
    void setSelectionProvider(std::function<QString()> f);
    // xuất .docx → .pdf bằng engine. Không có móc này thì lựa chọn PDF bị khoá và ghi rõ lý do.
    using PdfExporter = std::function<bool(const QString &docx, const QString &pdf, QString *error)>;
    void setPdfExporter(PdfExporter f);

    void setTemplatePath(const QString &docxPath);

private:
    struct Impl;
    Impl *d;
};

QWidget *createMailMergeWidget(QWidget *parent);
