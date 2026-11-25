// SPDX-License-Identifier: MPL-2.0
// Biểu tượng của khung giao diện (thanh bên, thanh trên, trang chủ) — MỌI lời gọi vẽ biểu tượng của
// khung đi qua đây, để sau này thay bằng bộ biểu tượng chung (dung::ui::icon) chỉ sửa một nơi.
#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace shell {

// Ô vuông bo góc nền `bg` với chữ cái `letter` (W, X, P, PDF, D...).
QPixmap badge(const QString& letter, const QColor& bg, const QColor& fg, int size, qreal dpr);
// Ô vuông bo góc nhạt với biểu tượng chung theo tên: home, docs, work, templates, projects, tools, open, about, search.
QPixmap symbol(const QString& name, const QColor& color, const QColor& bg, int size, qreal dpr);
QIcon icon(const QPixmap& pm);

} // namespace shell
