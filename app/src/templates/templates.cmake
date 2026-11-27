# Thư viện mẫu: nguồn cho DungOffice và bộ kiểm thử dung-templatetest (chỉ cần QtCore).
set(DUNG_TPL_DIR ${CMAKE_CURRENT_LIST_DIR})
target_sources(DungOffice PRIVATE
  ${DUNG_TPL_DIR}/TemplateLibrary.h ${DUNG_TPL_DIR}/TemplateLibrary.cpp
  ${DUNG_TPL_DIR}/TemplatesPage.h ${DUNG_TPL_DIR}/TemplatesPage.cpp)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-templatetest
  ${PROJECT_SOURCE_DIR}/tests/templates/template_tests.cpp
  ${DUNG_TPL_DIR}/TemplateLibrary.cpp ${DUNG_TPL_DIR}/TemplateLibrary.h
  ${DUNG_TPL_DIR}/../extras/WorkspaceStore.cpp ${DUNG_TPL_DIR}/../extras/WorkspaceStore.h
  ${DUNG_TPL_DIR}/../tools/filetools/ZipLite.cpp ${DUNG_TPL_DIR}/../tools/filetools/ZipLite.h)
target_include_directories(dung-templatetest PRIVATE ${DUNG_TPL_DIR} ${PROJECT_SOURCE_DIR}/app/src)
target_link_libraries(dung-templatetest PRIVATE Qt6::Core)
 set_target_properties(dung-templatetest PROPERTIES AUTOMOC OFF)
