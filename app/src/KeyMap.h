// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QKeyEvent>

// Chuyển phím Qt sang (charCode, keyCode) theo mã phím VCL mà LibreOfficeKit dùng.
struct LokKey {
    int charCode = 0;
    int keyCode = 0;
    bool valid = false;
};

LokKey toLokKey(const QKeyEvent* e);
int toLokModifiers(Qt::KeyboardModifiers m);   // KEY_SHIFT / KEY_MOD1 / KEY_MOD2
int toLokButtons(Qt::MouseButtons b);          // MOUSE_LEFT / MOUSE_MIDDLE / MOUSE_RIGHT
