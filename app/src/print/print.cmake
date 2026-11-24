# In và xem trước khi in: nguồn cho DungOffice và bộ kiểm thử dung-printtest.
set(DUNG_PRINT_DIR ${CMAKE_CURRENT_LIST_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
find_package(Qt6 6.4 QUIET OPTIONAL_COMPONENTS Pdf)

target_sources(DungOffice PRIVATE
  ${DUNG_PRINT_DIR}/PrintLogic.h ${DUNG_PRINT_DIR}/PrintLogic.cpp
  ${DUNG_PRINT_DIR}/PrintRender.h ${DUNG_PRINT_DIR}/PrintRender.cpp
  ${DUNG_PRINT_DIR}/PrintDialog.h ${DUNG_PRINT_DIR}/PrintDialog.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_PRINT_DIR})
target_link_libraries(DungOffice PRIVATE Qt6::PrintSupport)

add_executable(dung-printtest
  ${PROJECT_SOURCE_DIR}/tests/print/print_tests.cpp
  ${DUNG_PRINT_DIR}/PrintLogic.cpp ${DUNG_PRINT_DIR}/PrintLogic.h
  ${DUNG_PRINT_DIR}/PrintRender.cpp ${DUNG_PRINT_DIR}/PrintRender.h)
target_include_directories(dung-printtest PRIVATE ${DUNG_PRINT_DIR})
target_link_libraries(dung-printtest PRIVATE Qt6::Core Qt6::Gui Qt6::PrintSupport)
set_target_properties(dung-printtest PROPERTIES AUTOMOC OFF)

if(TARGET Qt6::Pdf)
  message(STATUS "In: có Qt6::Pdf — xem trước và in tích hợp")
  target_link_libraries(DungOffice PRIVATE Qt6::Pdf)
  target_link_libraries(dung-printtest PRIVATE Qt6::Pdf)
  target_compile_definitions(DungOffice PRIVATE DUNG_HAVE_QTPDF=1)
  target_compile_definitions(dung-printtest PRIVATE DUNG_HAVE_QTPDF=1)
else()
  message(WARNING "In: KHÔNG có Qt6::Pdf — chỉ mở bản in bằng trình xem PDF của máy")
endif()
