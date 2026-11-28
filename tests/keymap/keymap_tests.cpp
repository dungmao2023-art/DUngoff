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
    // 8. Thay ở chỗ không làm được bằng phím: từ chối toàn bộ, không chèn nhầm.
    { bool un = false; auto ops = imeToOps(QString("a"), 2, 3, &un);
      check(un && ops.empty(), "thay phần sau con trỏ xa: báo không hỗ trợ, không xoá; no partial insert"); }
    // Disjoint replacement to the left cannot be represented by adjacent Backspace.
    { bool un=false; auto ops=imeToOps(QString::fromUtf8("ệ"),-5,2,&un);
      check(un && ops.empty(), "IME disjoint left range: no adjacent deletion; no partial insert"); }
    { bool un=false; auto ops=imeToOps(QString("a"),-3,3,&un);
      check(!un && ops.size()==4 && ops[2].kind==ImeOp::Backspace, "IME contiguous left range remains supported"); }
    { bool un=false; auto ops=imeToOps(QString("a"),-2147483647-1,2147483647,&un);
      check(un && ops.empty(), "IME extreme range: no overflow or deletion; no partial insert"); }
    { bool un=false; auto ops=imeToOps(QString("a"),0,4097,&un);
      check(un && ops.empty(), "IME oversized replacement does not mass-delete; no partial insert"); }
    { QKeyEvent e(QEvent::KeyPress,Qt::Key_Backtab,Qt::NoModifier);
      auto k=toLokKey(&e); check(k.valid && k.keyCode==(1282|0x1000), "Backtab maps to Shift+Tab even without modifier"); }
    // 9. imePlan: bắt đầu soạn (chưa composing) -> một bước Preedit
    { bool ca = false; auto st = imePlan(QString::fromUtf8("d"), QString(), 0, 0, false, &ca);
      check(st.size() == 1 && st[0].kind == ImeStep::Preedit && st[0].text == "d" && ca, "imePlan: bắt đầu soạn 'd' -> Preedit, đang soạn"); }
    // 10. Cập nhật chữ đang soạn: chỉ Preedit mới, không End, không chèn thêm
    { bool ca = false; auto st = imePlan(QString::fromUtf8("dư"), QString(), 0, 0, true, &ca);
      check(st.size() == 1 && st[0].kind == ImeStep::Preedit && st[0].text == QString::fromUtf8("dư") && ca, "imePlan: cập nhật 'dư' -> chỉ Preedit"); }
    // 11. Chốt: commit thay chữ đang soạn rồi End; không gõ ký tự lần hai (không double-insert)
    { bool ca = true; auto st = imePlan(QString(), QString::fromUtf8("dư"), 0, 0, true, &ca);
      check(st.size() == 2 && st[0].kind == ImeStep::Preedit && st[0].text == QString::fromUtf8("dư") && st[1].kind == ImeStep::EndPreedit && !ca,
            "imePlan: chốt khi đang soạn -> Preedit(commit)+End, không Char"); }
    // 12. Chốt và mở cụm mới trong cùng sự kiện
    { bool ca = false; auto st = imePlan(QString("n"), QString::fromUtf8("á"), 0, 0, true, &ca);
      check(st.size() == 3 && st[1].kind == ImeStep::EndPreedit && st[2].kind == ImeStep::Preedit && st[2].text == "n" && ca, "imePlan: chốt rồi soạn cụm mới"); }
    // 13. Huỷ: preedit rỗng, không commit, đang soạn -> xoá chữ đang soạn rồi End
    { bool ca = true; auto st = imePlan(QString(), QString(), 0, 0, true, &ca);
      check(st.size() == 2 && st[0].kind == ImeStep::Preedit && st[0].text.isEmpty() && st[1].kind == ImeStep::EndPreedit && !ca, "imePlan: huỷ khi đang soạn"); }
    // 14. Không soạn, sự kiện rỗng: không làm gì
    { auto st = imePlan(QString(), QString(), 0, 0, false);
      check(st.empty(), "imePlan: sự kiện rỗng khi không soạn -> không làm gì"); }
    // 15. Không soạn, commit thường: ký tự trực tiếp như cũ (tương thích đường phím)
    { auto st = imePlan(QString(), QString::fromUtf8("Việt"), 0, 0, false);
      check(st.size() == 4 && st[0].kind == ImeStep::Char && st[2].charCode == 0x1EC7, "imePlan: commit khi không soạn -> Char như imeToOps"); }
    // 16. Không soạn, thay 3 ký tự trước con trỏ rồi soạn tiếp
    { auto st = imePlan(QString("o"), QString::fromUtf8("yễn"), -3, 3, false);
      check(st.size() == 7 && st[0].kind == ImeStep::Backspace && st[3].kind == ImeStep::Char && st[6].kind == ImeStep::Preedit, "imePlan: thay + chốt + soạn tiếp"); }
    // 17. Reject unsupported edits even if caller omits the diagnostic pointer.
    { bool un = false, ca = false; auto st = imePlan(QString("a"), QString("b"), -2, 2, true, &ca, &un);
      check(un && st.empty() && ca, "IME composing: reject full edit, preserve state until reset"); }
    { bool un = false, ca = true; auto st = imePlan(QString("a"), QString("b"), 2, 3, false, &ca, &un);
      check(un && st.empty() && !ca, "IME disjoint: no insert, deletion or new preedit"); }
    check(imePlan(QString("a"),QString("b"),-5,2,false).empty(), "IME rejection without diagnostics");
    check(imeToOps(QString("x"),2,0).empty(), "Reject insertion away from caret");
    check(imeToOps(QString("x"),0,-1).empty(), "Reject negative replacement length");
    std::cout << (g_fail ? "\nKẾT QUẢ: CÓ LỖI\\n" : "\nKẾT QUẢ: TẤT CẢ ĐẠT\\n");
    return g_fail ? 1 : 0;
}
