# Khối Excel — thêm nguồn vào app (include từ app/CMakeLists.txt)
target_sources(DungOffice PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/ExcelCommands.h
  ${CMAKE_CURRENT_LIST_DIR}/ExcelUi.cpp
)
