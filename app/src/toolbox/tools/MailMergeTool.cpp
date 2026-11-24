// SPDX-License-Identifier: MPL-2.0
// Đăng ký công cụ "Điền tên hàng loạt" vào Hộp công cụ.
#include "../ToolRegistry.h"
#include "../../tools/mailmerge/MailMergeWidget.h"

DUNG_REGISTER_TOOL(dien_ten_hang_loat, "Điền tên hàng loạt", "Điền tên/trường từ danh sách vào giấy mời, thư mời (docx, xlsx, pptx)", "Văn phòng", "✉", createMailMergeWidget)
