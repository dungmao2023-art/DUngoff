// Kiểm thử ánh xạ phím/bộ gõ → engine (QtGui, không cần màn hình). SPDX-License-Identifier: MPL-2.0
#include "KeyMap.h"
#include <QGuiApplication>
#include <iostream>

static int g_fail = 0;
static void check(bool ok, const char* what) { std::cout << (ok ? "[ĐẠT]  " : "[LỖI]  ") << what << std::endl; if (!ok) ++g_fail; }

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    // 1. Chữ có dấu gõ trực tiếp (bàn phím có sẵn dấu)
    { QKeyEvent e(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("ệ"));
      auto k = toLokKey(&e); check(k.valid && k.charCode == 0x1EC7, "phím gõ ra 'ệ' -> charCode U+1EC7"); }
    // 2. AltGr (Ctrl+Alt) + chữ in được: là gõ chữ, không phải phím tắt
    { QKeyEvent e(QEvent::KeyPress, Qt::Key_unknown, Qt::ControlModifier | Qt::AltModifier, QString::fromUtf8("đ"));
      auto k = toLokKey(&e); check(k.valid && k.charCode == 0x111, "AltGr + 'đ' vẫn gõ ra chữ (không bị nuốt)"); }
    // 3. Ctrl+C vẫn là phím tắt, không phát ra ký tự
    { QKeyEvent e(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier, QString("\x03"));
      auto k = toLokKey(&e); check(k.valid && k.charCode == 0, "Ctrl+C là phím tắt (không có ký tự)"); }
    // 4. Alt+F (phím tắt thuần): không phát ký tự
    { QKeyEvent e(QEvent::KeyPress, Qt::Key_F, Qt::AltModifier, QString());
      auto k = toLokKey(&e); check(k.charCode == 0, "Alt+F không phát ký tự"); }
    // 5. IME chốt chuỗi thường
    { auto ops = imeToOps(QString::fromUtf8("Việt"), 0, 0);
      check(ops.size() == 4 && ops[2].charCode == 0x1EC7, "IME chốt 'Việt' -> 4 ký tự"); }
    // 6. IME thay 3 ký tự trước con trỏ (Unikey/TSF): 3 Backspace rồi chữ mới
    { auto ops = imeToOps(QString::fromUtf8("yễn"), -3, 3);
      check(ops.size() == 6 && ops[0].kind == ImeOp::Backspace && ops[2].kind == ImeOp::Backspace && ops[3].kind == ImeOp::Char,
            "IME thay 3 ký tự trước con trỏ -> 3 Backspace + 3 ký tự"); }
    // 7. Thay cả phần sau con trỏ -> Delete
    { auto ops = imeToOps(QString("a"), -1, 3);
      check(ops.size() == 4 && ops[0].kind == ImeOp::Backspace && ops[1].kind == ImeOp::Delete && ops[3].kind == ImeOp::Char, "thay 1 trước + 2 sau -> BS + 2 Delete + ký tự"); }
    // 8. Thay ở chỗ không làm được bằng phím: báo unsupported, vẫn chốt chuỗi, không xoá bậy
    { bool un = false; auto ops = imeToOps(QString("a"), 2, 3, &un);
      check(un && ops.size() == 1 && ops[0].kind == ImeOp::Char, "thay phần sau con trỏ xa: báo không hỗ trợ, không xoá"); }
    std::cout << (g_fail ? "\nKẾT QUẢ: CÓ LỖI\\n" : "\nKẾT QUẢ: TẤT CẢ ĐẠT\\n");
    return g_fail ? 1 : 0;
}
