// SPDX-License-Identifier: MPL-2.0
// Công cụ chụp màn hình + chỉnh ảnh. Hợp đồng: tạo widget rồi nhúng vào nơi cần (nhánh feat/toolbox đăng ký);
// module này KHÔNG tự đăng ký vào menu hay trang nào.
#pragma once
#include <QImage>
#include <QString>
#include <functional>
class QWidget;

QWidget* createScreenshotWidget(QWidget* parent);

// Hỗ trợ kiểm thử (không dùng trong giao diện thật): nạp ảnh sẵn và lấy ảnh kết quả hiện tại.
void screenshotWidgetLoadImage(QWidget* w, const QImage& img);
QImage screenshotWidgetRender(QWidget* w);

// Nối nút "Chèn vào tài liệu đang mở": nơi nhúng cung cấp hàm nhận ảnh kết quả và trả chuỗi lỗi (rỗng = thành công).
// Chưa đăng ký thì nút ẩn (module không tự biết tài liệu nào đang mở).
void screenshotWidgetSetInsertHandler(QWidget* w, std::function<QString(const QImage&)> handler);
