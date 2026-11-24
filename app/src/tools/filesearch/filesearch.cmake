# Tìm kiếm tệp: lõi chỉ QtCore (bộ kiểm thử dung-filesearchtest) + widget createFileSearchWidget (chưa tự đăng ký vào menu).
set(DUNG_FS_DIR ${CMAKE_CURRENT_LIST_DIR})
target_sources(DungOffice PRIVATE
  ${DUNG_FS_DIR}/FileSearchCore.h ${DUNG_FS_DIR}/FileSearchCore.cpp
  ${DUNG_FS_DIR}/FileSearchWidget.h ${DUNG_FS_DIR}/FileSearchWidget.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_FS_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-filesearchtest
  ${PROJECT_SOURCE_DIR}/tests/filesearch/filesearch_tests.cpp
  ${DUNG_FS_DIR}/FileSearchCore.cpp ${DUNG_FS_DIR}/FileSearchCore.h)
target_include_directories(dung-filesearchtest PRIVATE ${DUNG_FS_DIR})
target_link_libraries(dung-filesearchtest PRIVATE Qt6::Core)
if(WIN32 AND MSVC)
  target_link_libraries(dung-filesearchtest PRIVATE psapi)
endif()
set_target_properties(dung-filesearchtest PROPERTIES AUTOMOC OFF)
