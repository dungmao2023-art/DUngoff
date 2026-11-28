// SPDX-License-Identifier: MPL-2.0
#include "KeyMap.h"

namespace {
// com::sun::star::awt::Key / vcl/keycodes.hxx
constexpr int KEY_0 = 256, KEY_A = 512, KEY_F1 = 768;
constexpr int KEY_DOWN = 1024, KEY_UP = 1025, KEY_LEFT = 1026, KEY_RIGHT = 1027, KEY_HOME = 1028,
              KEY_END = 1029, KEY_PAGEUP = 1030, KEY_PAGEDOWN = 1031;
constexpr int KEY_RETURN = 1280, KEY_ESCAPE = 1281, KEY_TAB = 1282, KEY_BACKSPACE = 1283,
              KEY_SPACE = 1284, KEY_INSERT = 1285, KEY_DELETE = 1286;
constexpr int KEY_SHIFT = 0x1000, KEY_MOD1 = 0x2000, KEY_MOD2 = 0x4000;
} // namespace

int toLokModifiers(Qt::KeyboardModifiers m) {
    int r = 0;
    if (m & Qt::ShiftModifier) r |= KEY_SHIFT;
    if (m & Qt::ControlModifier) r |= KEY_MOD1;
    if (m & Qt::AltModifier) r |= KEY_MOD2;
    return r;
}

int toLokButtons(Qt::MouseButtons b) {
    int r = 0;
    if (b & Qt::LeftButton) r |= 1;
    if (b & Qt::MiddleButton) r |= 2;
    if (b & Qt::RightButton) r |= 4;
    return r;
}

LokKey toLokKey(const QKeyEvent* e) {
    LokKey k;
    const int mods = toLokModifiers(e->modifiers());
    int code = 0;
    switch (e->key()) {
    case Qt::Key_Down: code = KEY_DOWN; break;
    case Qt::Key_Up: code = KEY_UP; break;
    case Qt::Key_Left: code = KEY_LEFT; break;
    case Qt::Key_Right: code = KEY_RIGHT; break;
    case Qt::Key_Home: code = KEY_HOME; break;
    case Qt::Key_End: code = KEY_END; break;
    case Qt::Key_PageUp: code = KEY_PAGEUP; break;
    case Qt::Key_PageDown: code = KEY_PAGEDOWN; break;
    case Qt::Key_Return:
    case Qt::Key_Enter: code = KEY_RETURN; break;
    case Qt::Key_Escape: code = KEY_ESCAPE; break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: code = KEY_TAB; break;
    case Qt::Key_Backspace: code = KEY_BACKSPACE; break;
    case Qt::Key_Insert: code = KEY_INSERT; break;
    case Qt::Key_Delete: code = KEY_DELETE; break;
    case Qt::Key_Space: code = KEY_SPACE; break;
    default:
        if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F24) code = KEY_F1 + (e->key() - Qt::Key_F1);
        break;
    }
    // AltGr (nhiều bàn phím/bộ gõ) được Windows báo là Ctrl+Alt: nếu có chữ in được thì đó là GÕ CHỮ, không phải phím tắt.
    const QString typed = e->text();
    const bool bothCtrlAlt = (e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::AltModifier);
    const bool altGrText = bothCtrlAlt && !typed.isEmpty() && typed.at(0).isPrint() && !typed.at(0).isSpace();
    const bool ctrlOrAlt = !altGrText && (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier));
    if (code) {
        k.keyCode = code | mods | (e->key() == Qt::Key_Backtab ? KEY_SHIFT : 0);
        if (code == KEY_SPACE && !ctrlOrAlt) k.charCode = ' ';
        if (code == KEY_RETURN && !ctrlOrAlt) k.charCode = '\r';
        k.valid = true;
        return k;
    }
    if (ctrlOrAlt) {
        // Phím tắt: không gửi ký tự, chỉ gửi mã phím + modifier.
        if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) { k.keyCode = (KEY_A + (e->key() - Qt::Key_A)) | mods; k.valid = true; }
        else if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9) { k.keyCode = (KEY_0 + (e->key() - Qt::Key_0)) | mods; k.valid = true; }
        return k;
    }
    const QString t = e->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        k.charCode = t.at(0).unicode();
        if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) k.keyCode = (KEY_A + (e->key() - Qt::Key_A)) | (mods & KEY_SHIFT);
        k.valid = true;
    }
    return k;
}

std::vector<ImeOp> imeToOps(const QString& commit, int replaceStart, int replaceLen, bool* unsupported) {
    std::vector<ImeOp> ops;
    if (unsupported) *unsupported = false;
    if (replaceLen < 0 || (replaceLen == 0 && replaceStart != 0)) {
        if (unsupported) *unsupported = true;
        return ops;
    }
    if (replaceLen > 0) {
        // Backspace/Delete can only replace a range touching the caret.
        // A disjoint range before it must not delete the adjacent characters.
        const long long end = static_cast<long long>(replaceStart) + replaceLen;
        if (replaceStart <= 0 && end >= 0 && replaceLen <= 4096) {
            const int before = -replaceStart;
            for (int i = 0; i < before; ++i) ops.push_back({ImeOp::Backspace, 0});
            for (int i = 0; i < replaceLen - before; ++i) ops.push_back({ImeOp::Delete, 0});
        } else {
            if (unsupported) *unsupported = true;
            return ops; // Reject the whole edit, including its commit string.
        }
    }
    for (const QChar c : commit) ops.push_back({ImeOp::Char, int(c.unicode())});
    return ops;
}

std::vector<ImeStep> imePlan(const QString& preedit, const QString& commit, int replaceStart, int replaceLen,
                             bool composing, bool* composingAfter, bool* unsupported) {
    std::vector<ImeStep> steps;
    if (unsupported) *unsupported = false;
    auto preeditStep = [](const QString& t) { ImeStep s; s.kind = ImeStep::Preedit; s.text = t; return s; };
    auto endStep = [] { ImeStep s; s.kind = ImeStep::EndPreedit; return s; };
    if (composing) {
        if (replaceLen != 0 || replaceStart != 0) {
            if (unsupported) *unsupported = true;
            if (composingAfter) *composingAfter = composing;
            return steps; // No partial commit of an unsupported replacement.
        }
        if (!commit.isEmpty()) {
            steps.push_back(preeditStep(commit));                      // commit thay chữ đang soạn
            steps.push_back(endStep());
            if (!preedit.isEmpty()) steps.push_back(preeditStep(preedit));  // bắt đầu cụm soạn mới ngay sau đó
        } else if (preedit.isEmpty()) {
            steps.push_back(preeditStep(QString()));                   // huỷ: xoá chữ đang soạn rồi kết thúc
            steps.push_back(endStep());
        } else {
            steps.push_back(preeditStep(preedit));                     // cập nhật chữ đang soạn
        }
        if (composingAfter) *composingAfter = !preedit.isEmpty();
        return steps;
    }
    bool rejected = false;
    const auto ops = imeToOps(commit, replaceStart, replaceLen, &rejected);
    if (rejected) {
        if (unsupported) *unsupported = true;
        if (composingAfter) *composingAfter = composing;
        return steps;
    }
    for (const ImeOp& op : ops) {
        ImeStep s;
        s.kind = op.kind == ImeOp::Backspace ? ImeStep::Backspace : op.kind == ImeOp::Delete ? ImeStep::Delete : ImeStep::Char;
        s.charCode = op.charCode;
        steps.push_back(s);
    }
    if (!preedit.isEmpty()) steps.push_back(preeditStep(preedit));
    if (composingAfter) *composingAfter = !preedit.isEmpty();
    return steps;
}
