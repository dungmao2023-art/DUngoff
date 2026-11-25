// SPDX-License-Identifier: MPL-2.0
// Dữ liệu vector của các biểu tượng (nội bộ). Định dạng: xem docs/ICONS.md.
#pragma once
namespace dung::ui {
struct IconDef { const char* name; const char* dsl; };
extern const IconDef kIconDefs[];
extern const int kIconDefCount;
} // namespace dung::ui
