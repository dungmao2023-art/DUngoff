# Tiện ích bảng tính (chỉ QtCore): bộ kiểm thử dung-tabletest.
set(DUNG_TBL_DIR ${CMAKE_CURRENT_LIST_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-tabletest
  ${PROJECT_SOURCE_DIR}/tests/tableutil/table_tests.cpp
  ${DUNG_TBL_DIR}/TableOps.cpp ${DUNG_TBL_DIR}/TableOps.h)
target_include_directories(dung-tabletest PRIVATE ${DUNG_TBL_DIR})
target_link_libraries(dung-tabletest PRIVATE Qt6::Core)
set_target_properties(dung-tabletest PROPERTIES AUTOMOC OFF)
