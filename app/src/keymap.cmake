# Bộ kiểm thử ánh xạ phím/bộ gõ (QtGui, chạy offscreen): dung-keymaptest.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui)
add_executable(dung-keymaptest
  ${PROJECT_SOURCE_DIR}/tests/keymap/keymap_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/KeyMap.cpp ${CMAKE_CURRENT_LIST_DIR}/KeyMap.h)
target_include_directories(dung-keymaptest PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dung-keymaptest PRIVATE Qt6::Core Qt6::Gui)
set_target_properties(dung-keymaptest PROPERTIES AUTOMOC OFF)
