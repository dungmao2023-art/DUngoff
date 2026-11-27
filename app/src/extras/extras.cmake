# Bảng bên phải (Note/Lịch/Liên hệ) và trang "Dự án khác".
target_sources(DungOffice PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/WorkspaceStore.h ${CMAKE_CURRENT_LIST_DIR}/WorkspaceStore.cpp
  ${CMAKE_CURRENT_LIST_DIR}/SidePanel.h ${CMAKE_CURRENT_LIST_DIR}/SidePanel.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ProjectAvatars.h
  ${CMAKE_CURRENT_LIST_DIR}/ProjectsPage.h ${CMAKE_CURRENT_LIST_DIR}/ProjectsPage.cpp)

# Offscreen project-directory regression: no network or production user data.
add_executable(dung-projecttest
  ${PROJECT_SOURCE_DIR}/tests/ui/project_page_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ProjectsPage.h ${CMAKE_CURRENT_LIST_DIR}/ProjectsPage.cpp
  ${CMAKE_CURRENT_LIST_DIR}/WorkspaceStore.h ${CMAKE_CURRENT_LIST_DIR}/WorkspaceStore.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ProjectAvatars.h)
target_link_libraries(dung-projecttest PRIVATE Qt6::Widgets)
set_target_properties(dung-projecttest PROPERTIES AUTOMOC ON)
