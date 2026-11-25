// SPDX-License-Identifier: MPL-2.0
// Đăng ký bộ công cụ tệp/thư mục/liên kết vào Hộp công cụ.
#include "../ToolRegistry.h"
#include "../../tools/filetools/FileToolsWidgets.h"

DUNG_REGISTER_TOOL(doi_ten_hang_loat, "Đổi tên hàng loạt", "Đổi tên nhiều tệp theo mẫu, có xem trước và hoàn tác", "Tệp & thư mục", "rename", createBatchRenameWidget)
DUNG_REGISTER_TOOL(tim_file_trung, "Tìm tệp trùng lặp", "Tìm các tệp giống hệt nhau; xoá chuyển vào thư mục _to_delete, hoàn tác được", "Tệp & thư mục", "duplicate-files", createDuplicateFinderWidget)
DUNG_REGISTER_TOOL(so_sanh_thu_muc, "So sánh hai thư mục", "Liệt kê tệp khác nhau, thiếu, mới hơn giữa hai thư mục", "Tệp & thư mục", "compare-folders", createFolderCompareWidget)
DUNG_REGISTER_TOOL(dung_luong_thu_muc, "Dung lượng thư mục", "Xem thư mục/tệp nào chiếm nhiều dung lượng", "Tệp & thư mục", "disk-usage", createDiskUsageWidget)
DUNG_REGISTER_TOOL(lam_sach_link, "Làm sạch liên kết", "Tìm và dọn lối tắt hỏng", "Liên kết", "link-check", createLinkToolsWidget)
DUNG_REGISTER_TOOL(sap_xep_tep, "Sắp xếp tệp vào thư mục", "Gom tệp theo loại, đuôi, năm hoặc tháng; có xem trước và hoàn tác", "Tệp & thư mục", "organize", createOrganizeWidget)
DUNG_REGISTER_TOOL(duong_dan_tep, "Đường dẫn tệp", "Sao chép đường dẫn nhiều dạng; tìm đường dẫn dài hơn 260 ký tự", "Tệp & thư mục", "path", createPathToolsWidget)
DUNG_REGISTER_TOOL(quet_lien_ket, "Quét liên kết hỏng", "Kiểm tra lối tắt và liên kết trong docx/xlsx/pptx", "Liên kết", "link", createLinkScanWidget)
