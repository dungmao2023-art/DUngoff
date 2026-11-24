# Nguồn của công cụ PowerPoint (nhánh feat/powerpoint)
target_sources(DungOffice PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/SlideShow.h ${CMAKE_CURRENT_LIST_DIR}/SlideShow.cpp
  ${CMAKE_CURRENT_LIST_DIR}/SlideThumbs.h ${CMAKE_CURRENT_LIST_DIR}/SlideThumbs.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PptActions.h ${CMAKE_CURRENT_LIST_DIR}/PptActions.cpp
)
