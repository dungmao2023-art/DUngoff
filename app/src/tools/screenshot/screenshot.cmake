# Công cụ chụp màn hình + chỉnh ảnh. Lõi (QtGui) có bộ kiểm thử riêng dung-shottest; widget có dung-shotwidgettest.
set(DUNG_SHOT_DIR ${CMAKE_CURRENT_LIST_DIR})
target_sources(DungOffice PRIVATE
  ${DUNG_SHOT_DIR}/ScreenshotCore.h ${DUNG_SHOT_DIR}/ScreenshotCore.cpp
  ${DUNG_SHOT_DIR}/ScreenshotWidget.h ${DUNG_SHOT_DIR}/ScreenshotWidget.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_SHOT_DIR})
if(WIN32)
  target_link_libraries(DungOffice PRIVATE dwmapi user32)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Widgets)
add_executable(dung-shottest
  ${PROJECT_SOURCE_DIR}/tests/screenshot/shot_tests.cpp
  ${DUNG_SHOT_DIR}/ScreenshotCore.cpp ${DUNG_SHOT_DIR}/ScreenshotCore.h)
target_include_directories(dung-shottest PRIVATE ${DUNG_SHOT_DIR})
target_link_libraries(dung-shottest PRIVATE Qt6::Gui)
set_target_properties(dung-shottest PROPERTIES AUTOMOC OFF)

add_executable(dung-shotwidgettest
  ${PROJECT_SOURCE_DIR}/tests/screenshot/shot_widget_tests.cpp
  ${DUNG_SHOT_DIR}/ScreenshotCore.cpp ${DUNG_SHOT_DIR}/ScreenshotCore.h
  ${DUNG_SHOT_DIR}/ScreenshotWidget.cpp ${DUNG_SHOT_DIR}/ScreenshotWidget.h)
target_include_directories(dung-shotwidgettest PRIVATE ${DUNG_SHOT_DIR})
target_link_libraries(dung-shotwidgettest PRIVATE Qt6::Widgets)
if(WIN32)
  target_link_libraries(dung-shotwidgettest PRIVATE dwmapi user32)
endif()
set_target_properties(dung-shotwidgettest PROPERTIES AUTOMOC ON)
