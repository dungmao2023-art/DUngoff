// SPDX-License-Identifier: MPL-2.0
// Đăng ký công cụ tìm từ khoá trong thư mục vào Hộp công cụ.
#include "../ToolRegistry.h"
#include "../../tools/filesearch/FileSearchWidget.h"

DUNG_REGISTER_TOOL(tim_trong_thu_muc, "Tìm trong thư mục", "Tìm theo tên và nội dung file, không phân biệt dấu tiếng Việt", "Tệp & thư mục", "folder-search", createFileSearchWidget)
