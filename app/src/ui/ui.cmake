# Hệ biểu tượng vector tự chứa (không dùng font biểu tượng, không cần Qt6::Svg).
set(DUNG_UI_DIR ${CMAKE_CURRENT_LIST_DIR})
set(DUNG_UI_SRC
  ${DUNG_UI_DIR}/Icons.h ${DUNG_UI_DIR}/Icons.cpp
  ${DUNG_UI_DIR}/IconData.h ${DUNG_UI_DIR}/IconData.cpp
  ${DUNG_UI_DIR}/CommandIcons.cpp)
target_sources(DungOffice PRIVATE ${DUNG_UI_SRC})
target_include_directories(DungOffice PRIVATE ${DUNG_UI_DIR})

find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Widgets)
# Kiểm thử lõi (QImage, không cần màn hình): mọi icon vẽ được, khác nhau, Disabled khác Normal, các cỡ/DPR, tờ tổng hợp PNG.
add_executable(dung-icontest ${PROJECT_SOURCE_DIR}/tests/ui/icon_tests.cpp ${DUNG_UI_SRC})
target_include_directories(dung-icontest PRIVATE ${DUNG_UI_DIR})
target_link_libraries(dung-icontest PRIVATE Qt6::Gui)
set_target_properties(dung-icontest PROPERTIES AUTOMOC OFF)

# Kiểm thử ribbon: mọi mã lệnh trên ribbon đều có icon hợp lệ (widget, offscreen).
add_executable(dung-ribbonicontest
  ${PROJECT_SOURCE_DIR}/tests/ui/ribbon_icon_tests.cpp
  ${DUNG_UI_SRC}
  ${DUNG_UI_DIR}/../Ribbon.h ${DUNG_UI_DIR}/../Ribbon.cpp ${DUNG_UI_DIR}/../word/WordRibbon.cpp
  ${DUNG_UI_DIR}/../Theme.h ${DUNG_UI_DIR}/../Theme.cpp
  ${DUNG_UI_DIR}/../word/WordCommands.h ${DUNG_UI_DIR}/../word/WordCommands.cpp)
target_include_directories(dung-ribbonicontest PRIVATE ${DUNG_UI_DIR} ${DUNG_UI_DIR}/.. ${PROJECT_SOURCE_DIR}/engine/include)
target_link_libraries(dung-ribbonicontest PRIVATE Qt6::Widgets)
set_target_properties(dung-ribbonicontest PROPERTIES AUTOMOC ON)
