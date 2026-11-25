// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QColor>
#include <QFont>
#include <QString>

namespace theme {

inline const QColor kBlue{0x28, 0x64, 0xBA};       // xanh chủ đạo (theo thiết kế)
inline const QColor kBlueDark{0x1B, 0x4A, 0x90};
inline const QColor kBlueLight{0xE8, 0xF0, 0xFC};
inline const QColor kBorder{0xDC, 0xE5, 0xF2};
inline const QColor kText{0x17, 0x29, 0x4D};
inline const QColor kMuted{0x6B, 0x7A, 0x99};
inline const QColor kExcel{0x16, 0x80, 0x4E};
inline const QColor kWord{0x28, 0x64, 0xBA};
inline const QColor kPowerPoint{0xC4, 0x55, 0x30};
inline const QColor kPdf{0xB9, 0x44, 0x49};

// Màu nhấn theo loại: "word", "excel", "ppt", "pdf" (khác -> xanh chủ đạo).
QColor accentFor(const QString& kind);

QString styleSheet();
// Font biểu tượng (Segoe Fluent Icons / Segoe MDL2 Assets trên Windows). Rỗng nếu không có.
QString iconFontFamily();
// Trả về ký tự glyph nếu có font biểu tượng, ngược lại ký tự thay thế.
QString glyph(char16_t mdl2, const QString& fallback);

} // namespace theme
