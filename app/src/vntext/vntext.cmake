# Tiện ích văn bản tiếng Việt (số tiền thành chữ, ngày, bảng mã cũ): chỉ cần QtCore, có bộ kiểm thử riêng.
set(DUNG_VNTEXT_DIR ${CMAKE_CURRENT_LIST_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-vntexttest
  ${PROJECT_SOURCE_DIR}/tests/vntext/vntext_tests.cpp
  ${DUNG_VNTEXT_DIR}/VnText.cpp ${DUNG_VNTEXT_DIR}/VnText.h)
target_include_directories(dung-vntexttest PRIVATE ${DUNG_VNTEXT_DIR} ${PROJECT_SOURCE_DIR}/app/src)
target_link_libraries(dung-vntexttest PRIVATE Qt6::Core)
set_target_properties(dung-vntexttest PROPERTIES AUTOMOC OFF)

# Kiểm tra chính tả theo âm tiết (luật âm vị; không dùng từ điển bên thứ ba)
add_executable(dung-syllabletest
  ${PROJECT_SOURCE_DIR}/tests/vntext/syllable_tests.cpp
  ${DUNG_VNTEXT_DIR}/SyllableCheck.cpp ${DUNG_VNTEXT_DIR}/SyllableCheck.h)
target_include_directories(dung-syllabletest PRIVATE ${DUNG_VNTEXT_DIR} ${PROJECT_SOURCE_DIR}/app/src)
target_link_libraries(dung-syllabletest PRIVATE Qt6::Core)
set_target_properties(dung-syllabletest PROPERTIES AUTOMOC OFF)
