# Tiện ích văn bản tiếng Việt (số tiền thành chữ, ngày, bảng mã cũ): chỉ cần QtCore, có bộ kiểm thử riêng.
set(DUNG_VNTEXT_DIR ${CMAKE_CURRENT_LIST_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-vntexttest
  ${PROJECT_SOURCE_DIR}/tests/vntext/vntext_tests.cpp
  ${DUNG_VNTEXT_DIR}/VnText.cpp ${DUNG_VNTEXT_DIR}/VnText.h)
target_include_directories(dung-vntexttest PRIVATE ${DUNG_VNTEXT_DIR} ${PROJECT_SOURCE_DIR}/app/src)
target_link_libraries(dung-vntexttest PRIVATE Qt6::Core)
set_target_properties(dung-vntexttest PROPERTIES AUTOMOC OFF)
