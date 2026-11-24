# Điền tên hàng loạt (trộn thư): lõi + widget; bộ kiểm thử dung-mailmergetest (QtCore + QZipReader riêng của Qt Gui).
set(DUNG_MM_DIR ${CMAKE_CURRENT_LIST_DIR}/mailmerge)
target_sources(DungOffice PRIVATE
  ${DUNG_MM_DIR}/MailMergeCore.h ${DUNG_MM_DIR}/MailMergeCore.cpp
  ${DUNG_MM_DIR}/MailMergeWidget.h ${DUNG_MM_DIR}/MailMergeWidget.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_MM_DIR})

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui)
target_link_libraries(DungOffice PRIVATE Qt6::GuiPrivate)  # QZipReader/QZipWriter (API riêng của Qt, đọc/ghi zip UTF-8)
add_executable(dung-mailmergetest
  ${PROJECT_SOURCE_DIR}/tests/mailmerge/mailmerge_tests.cpp
  ${DUNG_MM_DIR}/MailMergeCore.cpp ${DUNG_MM_DIR}/MailMergeCore.h)
target_include_directories(dung-mailmergetest PRIVATE ${DUNG_MM_DIR})
target_link_libraries(dung-mailmergetest PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate)
set_target_properties(dung-mailmergetest PROPERTIES AUTOMOC OFF)
