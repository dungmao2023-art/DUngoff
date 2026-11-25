// SPDX-License-Identifier: MPL-2.0
// Đăng ký công cụ chụp màn hình vào Hộp công cụ.
#include "../ToolRegistry.h"
#include "../../tools/screenshot/ScreenshotWidget.h"

DUNG_REGISTER_TOOL(chup_man_hinh, "Chụp màn hình", "Chụp vùng/cửa sổ/toàn màn hình, chú thích, làm mờ thông tin nhạy cảm", "Tiện ích", "screenshot", createScreenshotWidget)
