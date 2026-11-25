// SPDX-License-Identifier: MPL-2.0
// Hệ biểu tượng tự chứa của Dũng Office: mọi icon là hình vector tự vẽ (xem IconData.cpp),
// kết xuất bằng QPainter — KHÔNG phụ thuộc font biểu tượng (Segoe MDL2/Fluent), emoji hay Qt6::Svg.
// Dùng:  button->setIcon(dung::ui::icon("bold"));   hoặc   dung::ui::pixmap("home", Qt::white, 24, dpr)
#pragma once
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace dung::ui {

// Màu mặc định của icon khi không truyền màu (xanh đậm của giao diện).
QColor defaultIconColor();
// Màu icon ở trạng thái vô hiệu.
QColor disabledIconColor();

// QIcon vẽ lại theo đúng kích thước/DPI yêu cầu (QIconEngine) — sắc nét ở mọi tỷ lệ. Tên lạ → hình khung trống.
// Trạng thái Disabled tự làm nhạt. `logicalSize` chỉ là gợi ý kích thước mặc định.
QIcon icon(const QString& name, const QColor& color = QColor(), int logicalSize = 24);
QPixmap pixmap(const QString& name, const QColor& color, int logicalSize, qreal dpr = 1.0);
// Kết xuất thô ra QImage ARGB32_Premultiplied cỡ pixelSize x pixelSize (dùng cho kiểm thử, không cần QGuiApplication).
QImage renderImage(const QString& name, const QColor& color, int pixelSize);

bool hasIcon(const QString& name);
QStringList iconNames();   // tất cả tên, theo thứ tự đăng ký

// Tên icon cho một mã lệnh của ribbon/thanh bên (".uno:Bold", "word:table", "xl:find", "pdfTools:merge"...).
// Trả chuỗi rỗng nếu chưa có ánh xạ (kiểm thử sẽ báo).
QString iconNameForCommand(const QString& commandId);

// Icon cho ToolInfo.iconText: nếu chuỗi trùng tên icon thì vẽ icon, ngược lại QIcon rỗng (hiện chữ).
QIcon iconOrEmpty(const QString& nameOrText, const QColor& color = QColor());

} // namespace dung::ui
