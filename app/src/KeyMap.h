// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QKeyEvent>
#include <QString>
#include <vector>

// Chuyển phím Qt sang (charCode, keyCode) theo mã phím VCL mà LibreOfficeKit dùng.
struct LokKey {
    int charCode = 0;
    int keyCode = 0;
    bool valid = false;
};

LokKey toLokKey(const QKeyEvent* e);
int toLokModifiers(Qt::KeyboardModifiers m);   // KEY_SHIFT / KEY_MOD1 / KEY_MOD2
int toLokButtons(Qt::MouseButtons b);          // MOUSE_LEFT / MOUSE_MIDDLE / MOUSE_RIGHT

// Bộ gõ (IME/TSF) → chuỗi thao tác phím gửi engine. Chỉ thuần dữ liệu để kiểm thử được không cần giao diện.
struct ImeOp {
    enum Kind { Char, Backspace, Delete } kind = Char;
    int charCode = 0;   // đơn vị UTF-16 (kind == Char)
};
// commit: chuỗi bộ gõ chốt; replaceStart/replaceLen: phần văn bản bộ gõ muốn thay (tính từ con trỏ, theo QInputMethodEvent).
// Chỉ hỗ trợ thay phần NẰM TRƯỚC con trỏ (replaceStart <= 0) bằng Backspace và phần sau bằng Delete;
// thay ở chỗ khác không làm được qua phím -> đặt *unsupported = true và bỏ phần thay (vẫn chốt chuỗi).
std::vector<ImeOp> imeToOps(const QString& commit, int replaceStart, int replaceLen, bool* unsupported = nullptr);

// Kế hoạch xử lý MỘT QInputMethodEvent, gồm cả chữ đang soạn (preedit). Hàm thuần để kiểm thử không cần giao diện.
//  - Preedit: đặt/thay chữ đang soạn trong engine (LOK_EXT_TEXTINPUT, type 0).
//  - EndPreedit: chốt chữ đang soạn đúng như đang hiển thị (LOK_EXT_TEXTINPUT_END, type 2).
//  - Char/Backspace/Delete: như imeToOps (đường phím trực tiếp, dùng khi KHÔNG đang soạn).
struct ImeStep {
    enum Kind { Char, Backspace, Delete, Preedit, EndPreedit } kind = Char;
    int charCode = 0;   // kind == Char
    QString text;       // kind == Preedit
};
// preedit/commit/replaceStart/replaceLen: lấy từ QInputMethodEvent. composing: engine đang có chữ đang soạn.
// *composingAfter: sau khi chạy các bước, engine còn chữ đang soạn hay không.
// *unsupported: đã bỏ phần "thay thế" vì không làm được (đang soạn dở, hoặc thay ở chỗ xa con trỏ).
// Quy ước: đang soạn mà có commit -> commit THAY chữ đang soạn rồi chốt (không chèn hai lần);
//          đang soạn mà preedit rỗng và không commit -> HUỶ chữ đang soạn (engine trả về như trước khi soạn).
std::vector<ImeStep> imePlan(const QString& preedit, const QString& commit, int replaceStart, int replaceLen,
                             bool composing, bool* composingAfter = nullptr, bool* unsupported = nullptr);
