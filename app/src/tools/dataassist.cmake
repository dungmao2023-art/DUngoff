# Tiện ích dữ liệu theo bản thiết kế HTML: ƒx Tôi muốn, thống kê/kiểm tra vùng, xuất theo mẫu, kho luật.
# Lõi (FormulaCatalog, DataChecks, TemplateExport) kiểm thử bằng dung-dataassisttest.
set(DUNG_DA_DIR ${CMAKE_CURRENT_LIST_DIR}/dataassist)
set(DUNG_DA_CORE
  ${DUNG_DA_DIR}/FormulaCatalog.h ${DUNG_DA_DIR}/FormulaCatalog.cpp
  ${DUNG_DA_DIR}/DataChecks.h ${DUNG_DA_DIR}/DataChecks.cpp
  ${DUNG_DA_DIR}/TemplateExport.h ${DUNG_DA_DIR}/TemplateExport.cpp)
target_sources(DungOffice PRIVATE ${DUNG_DA_CORE}
  ${DUNG_DA_DIR}/DataAssistWidgets.h ${DUNG_DA_DIR}/DataAssistWidgets.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../toolbox/tools/DataAssistTool.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_DA_DIR} ${CMAKE_CURRENT_LIST_DIR}/../tableutil ${CMAKE_CURRENT_LIST_DIR}/mailmerge)

add_executable(dung-dataassisttest
  ${PROJECT_SOURCE_DIR}/tests/dataassist/dataassist_tests.cpp ${DUNG_DA_CORE})
target_include_directories(dung-dataassisttest PRIVATE ${DUNG_DA_DIR})
target_link_libraries(dung-dataassisttest PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate)
set_target_properties(dung-dataassisttest PROPERTIES AUTOMOC OFF)
