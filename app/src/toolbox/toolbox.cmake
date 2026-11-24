# Hộp công cụ: khung đăng ký công cụ + tool ngoài (tool.json). Bộ kiểm thử dung-toolboxtest (chỉ QtCore).
set(DUNG_TB_DIR ${CMAKE_CURRENT_LIST_DIR})

# Phần lõi của khung
target_sources(DungOffice PRIVATE
  ${DUNG_TB_DIR}/ToolRegistry.h ${DUNG_TB_DIR}/ToolRegistry.cpp
  ${DUNG_TB_DIR}/ExternalTool.h ${DUNG_TB_DIR}/ExternalTool.cpp
  ${DUNG_TB_DIR}/ExternalToolWidget.h ${DUNG_TB_DIR}/ExternalToolWidget.cpp
  ${DUNG_TB_DIR}/ToolboxPage.h ${DUNG_TB_DIR}/ToolboxPage.cpp)

# ==== DANH SÁCH CÔNG CỤ — thêm công cụ mới = thêm 1 dòng vào đây ====
# (Liệt kê ở target_sources của DungOffice nên trình liên kết không bỏ sót tệp tự đăng ký.)
target_sources(DungOffice PRIVATE
  ${DUNG_TB_DIR}/tools/TimerTool.cpp         # Đồng hồ (công cụ mẫu)
  # Các công cụ do nhánh khác viết — mỗi công cụ cung cấp  QWidget* create<Ten>Widget(QWidget* parent)
  # và tự đăng ký bằng DUNG_REGISTER_TOOL trong tệp .cpp của nó. Bỏ dấu # khi tệp đã có trong repo:
  # ${DUNG_TB_DIR}/tools/MailMergeTool.cpp   # createMailMergeWidget   — điền tên hàng loạt vào giấy mời
  # ${DUNG_TB_DIR}/tools/ScreenshotTool.cpp  # createScreenshotWidget  — chụp và chỉnh sửa màn hình
  # ${DUNG_TB_DIR}/tools/FileSearchTool.cpp  # createFileSearchWidget  — tìm từ khoá trong thư mục
  # ${DUNG_TB_DIR}/tools/FileToolsTool.cpp   # createFileToolsWidget   — công cụ làm việc với tệp/thư mục/liên kết
)
target_include_directories(DungOffice PRIVATE ${DUNG_TB_DIR})

# Bộ kiểm thử (không giao diện)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-toolboxtest
  ${PROJECT_SOURCE_DIR}/tests/toolbox/toolbox_tests.cpp
  ${DUNG_TB_DIR}/ToolRegistry.h ${DUNG_TB_DIR}/ToolRegistry.cpp
  ${DUNG_TB_DIR}/ExternalTool.h ${DUNG_TB_DIR}/ExternalTool.cpp
  ${DUNG_TB_DIR}/tools/TimerFormat.h
  ${DUNG_TB_DIR}/../extras/WorkspaceStore.cpp ${DUNG_TB_DIR}/../extras/WorkspaceStore.h)
target_include_directories(dung-toolboxtest PRIVATE ${DUNG_TB_DIR} ${PROJECT_SOURCE_DIR}/app/src)
target_link_libraries(dung-toolboxtest PRIVATE Qt6::Core)
target_compile_definitions(dung-toolboxtest PRIVATE DUNG_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples/tools")
