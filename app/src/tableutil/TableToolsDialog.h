// SPDX-License-Identifier: MPL-2.0
// Hộp thoại "Tiện ích bảng": chọn CSV, xem trước bảng mã (bắt xác nhận khi không chắc), rồi chạy
// xoá dòng trùng / làm sạch ngày-tiền / so sánh hai CSV. Luôn ra BẢN SAO mới + báo cáo; không bao giờ ghi đè tệp gốc.
#pragma once
#include <QDialog>

class TableToolsDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { Dedup = 0, Clean = 1, Compare = 2 };
    static void run(QWidget* parent, Tab tab = Dedup, const QString& initialPath = QString());

private:
    explicit TableToolsDialog(QWidget* parent);
    struct Impl;
    Impl* d;
};
