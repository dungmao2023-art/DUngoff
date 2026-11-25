# Kiểm thử con trỏ nhập liệu: dung-carettest (đơn vị + TileView thật trên engine, nền offscreen).
add_executable(dung-carettest
  ${PROJECT_SOURCE_DIR}/tests/caret/caret_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/TileView.h ${CMAKE_CURRENT_LIST_DIR}/TileView.cpp
  ${CMAKE_CURRENT_LIST_DIR}/KeyMap.h ${CMAKE_CURRENT_LIST_DIR}/KeyMap.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Theme.h ${CMAKE_CURRENT_LIST_DIR}/Theme.cpp
  ${CMAKE_CURRENT_LIST_DIR}/CaretGeom.h ${CMAKE_CURRENT_LIST_DIR}/CaretTrace.h)
target_include_directories(dung-carettest PRIVATE ${PROJECT_SOURCE_DIR}/engine/include ${PROJECT_SOURCE_DIR}/third_party/libreofficekit/include)
target_compile_definitions(dung-carettest PRIVATE DUNG_CARET_WITH_ENGINE)
target_link_libraries(dung-carettest PRIVATE dungengine Qt6::Widgets)
set_target_properties(dung-carettest PROPERTIES AUTOMOC ON)
