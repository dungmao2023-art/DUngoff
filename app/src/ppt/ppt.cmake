# Nguồn của công cụ PowerPoint (nhánh feat/powerpoint)
target_sources(DungOffice PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/SlideShow.h ${CMAKE_CURRENT_LIST_DIR}/SlideShow.cpp
  ${CMAKE_CURRENT_LIST_DIR}/SlideThumbs.h ${CMAKE_CURRENT_LIST_DIR}/SlideThumbs.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PptActions.h ${CMAKE_CURRENT_LIST_DIR}/PptActions.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PptTransition.h ${CMAKE_CURRENT_LIST_DIR}/PptTransition.cpp
)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-ppttransitiontest
  ${PROJECT_SOURCE_DIR}/tests/ppt/transition_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PptTransition.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../word/PageMargins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../tools/filetools/ZipLite.cpp)
target_include_directories(dung-ppttransitiontest PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dung-ppttransitiontest PRIVATE Qt6::Core)
set_target_properties(dung-ppttransitiontest PROPERTIES AUTOMOC OFF)

# Real GUI playback test using the existing engine and generated PPTX fixture.
add_executable(dung-pptplaybacktest
  ${PROJECT_SOURCE_DIR}/tests/ppt/playback_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/SlideShow.cpp ${CMAKE_CURRENT_LIST_DIR}/SlideShow.h
  ${CMAKE_CURRENT_LIST_DIR}/PptTransition.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../word/PageMargins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../tools/filetools/ZipLite.cpp)
target_include_directories(dung-pptplaybacktest PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dung-pptplaybacktest PRIVATE Qt6::Widgets dungengine)
set_target_properties(dung-pptplaybacktest PROPERTIES AUTOMOC ON)
