find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(DungOffice WIN32
  src/main.cpp
  src/Theme.h src/Theme.cpp
  src/EngineHost.h src/EngineHost.cpp
  src/KeyMap.h src/KeyMap.cpp
  src/TileView.h src/TileView.cpp
  src/Ribbon.h src/Ribbon.cpp
  src/DocumentTab.h src/DocumentTab.cpp
  src/HomePage.h src/HomePage.cpp
  src/MainWindow.h src/MainWindow.cpp
  resources/app.qrc
)
include(src/excel/excel.cmake)  # khối Excel
if(WIN32)
  target_sources(DungOffice PRIVATE resources/app.rc)
endif()

include(src/ppt/ppt.cmake)
include(src/extras/extras.cmake)
include(src/templates/templates.cmake)
include(src/tableutil/tableutil.cmake)
include(src/recovery/recovery.cmake)
include(src/vntext/vntext.cmake)
include(src/keymap.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/src/pdf/pdf.cmake)
target_link_libraries(DungOffice PRIVATE dungengine Qt6::Widgets)
target_include_directories(DungOffice PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libreofficekit/include)
target_compile_definitions(DungOffice PRIVATE DUNG_VERSION="${PROJECT_VERSION}")
set_target_properties(DungOffice PROPERTIES OUTPUT_NAME "DungOffice")
