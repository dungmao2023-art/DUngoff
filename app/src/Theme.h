// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QColor>
#include <QFont>
#include <QString>

namespace theme {

inline const QColor kBlue{0x1F, 0x5F, 0xBF};       // xanh chủ đạo
inline const QColor kBlueDark{0x16, 0x47, 0x8F};
inline const QColor kBlueLight{0xE8, 0xF0, 0xFC};
inline const QColor kBorder{0xD9, 0xE1, 0xEC};
inline const QColor kText{0x1E, 0x29, 0x3B};
inline const QColor kMuted{0x64, 0x74, 0x8B};
inline const QColor kExcel{0x1E, 0x8E, 0x4F};
inline const QColor kWord{0x2B, 0x67, 0xD1};
inline const QColor kPowerPoint{0xD2, 0x55, 0x2E};
inline const QColor kPdf{0xC6, 0x28, 0x28};

QString styleSheet();
// Font biểu tượng (Segoe Fluent Icons / Segoe MDL2 Assets trên Windows). Rỗng nếu không có.
QString iconFontFamily();
// Trả về ký tự glyph nếu có font biểu tượng, ngược lại ký tự thay thế.
QString glyph(char16_t mdl2, const QString& fallback);

} // namespace theme
