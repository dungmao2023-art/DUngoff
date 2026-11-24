# Công cụ tệp/thư mục/liên kết. Lõi FileOps chỉ cần QtCore → bộ kiểm thử dung-filetest.
set(DUNG_FT_DIR ${CMAKE_CURRENT_LIST_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network)
target_sources(DungOffice PRIVATE
  ${DUNG_FT_DIR}/FileOps.h ${DUNG_FT_DIR}/FileOps.cpp
  ${DUNG_FT_DIR}/ZipLite.h ${DUNG_FT_DIR}/ZipLite.cpp
  ${DUNG_FT_DIR}/LinkScan.h ${DUNG_FT_DIR}/LinkScan.cpp
  ${DUNG_FT_DIR}/FileToolsWidgets.h ${DUNG_FT_DIR}/FileToolsWidgets.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_FT_DIR})
target_link_libraries(DungOffice PRIVATE Qt6::Network)
add_executable(dung-filetest
  ${PROJECT_SOURCE_DIR}/tests/filetools/file_tests.cpp
  ${DUNG_FT_DIR}/FileOps.cpp ${DUNG_FT_DIR}/FileOps.h
  ${DUNG_FT_DIR}/ZipLite.cpp ${DUNG_FT_DIR}/ZipLite.h ${DUNG_FT_DIR}/LinkScan.cpp ${DUNG_FT_DIR}/LinkScan.h)
target_include_directories(dung-filetest PRIVATE ${DUNG_FT_DIR})
target_link_libraries(dung-filetest PRIVATE Qt6::Core)
set_target_properties(dung-filetest PROPERTIES AUTOMOC OFF)
