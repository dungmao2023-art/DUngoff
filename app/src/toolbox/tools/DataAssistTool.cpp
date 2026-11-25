// SPDX-License-Identifier: MPL-2.0
// Đăng ký các tiện ích dữ liệu (theo bản thiết kế HTML) vào Hộp công cụ.
#include "../ToolRegistry.h"
#include "../../tools/dataassist/DataAssistWidgets.h"

DUNG_REGISTER_TOOL(fx_toi_muon, "ƒx · Tôi muốn", "Tìm hàm theo việc cần làm, điền đối số, chèn công thức (engine tính)", "Excel", "formula", createFormulaAssistantWidget)
DUNG_REGISTER_TOOL(cong_cu_vung, "Công cụ vùng dữ liệu", "Thống kê vùng, kiểm tra ô lỗi công thức, tìm dòng trùng, biểu đồ nhanh", "Excel", "table", createRangeToolsWidget)
DUNG_REGISTER_TOOL(xuat_theo_mau, "Xuất theo mẫu (MISA, FAST, KiotViet, Sapo)", "Ghép cột vào mẫu .xlsx do bạn cung cấp, ghi tệp mới", "Excel", "export", createTemplateExportWidget)
DUNG_REGISTER_TOOL(kho_luat, "Kho luật", "Liên kết tới nguồn văn bản pháp luật (chưa xác minh hiệu lực)", "Tham khảo", "law", createLawLinksWidget)
