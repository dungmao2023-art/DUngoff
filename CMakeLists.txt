cmake_minimum_required(VERSION 3.21)
project(DungOffice VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DUNG_BUILD_APP "Build giao diện Dũng Office (cần Qt 6)" ON)
option(DUNG_BUILD_TESTS "Build công cụ kiểm thử dung-smoketest" ON)

if(MSVC)
  add_compile_options(/utf-8 /W3 /permissive-)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- dung_api.py nhúng vào engine dưới dạng bảng byte (tránh giới hạn độ dài chuỗi của MSVC) ----
set(DUNG_API_PY ${CMAKE_CURRENT_SOURCE_DIR}/engine-build/dung-scripts/dung_api.py)
set(DUNG_API_INC ${CMAKE_CURRENT_BINARY_DIR}/gen/dung_api_py.inc)
file(READ ${DUNG_API_PY} DUNG_API_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," DUNG_API_HEX "${DUNG_API_HEX}")
file(WRITE ${DUNG_API_INC} "${DUNG_API_HEX}\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${DUNG_API_PY})

# ---- engine: bọc LibreOfficeKit, không phụ thuộc giao diện ----
add_library(dungengine STATIC
  engine/src/Engine.cpp
  engine/src/FormatPolicy.cpp)
target_include_directories(dungengine
  PUBLIC engine/include
  PRIVATE third_party/libreofficekit/include ${CMAKE_CURRENT_BINARY_DIR}/gen)
if(UNIX)
  target_link_libraries(dungengine PUBLIC ${CMAKE_DL_LIBS})
endif()

if(DUNG_BUILD_TESTS)
  add_executable(dung-smoketest tools/smoketest/main.cpp tools/smoketest/ppt_tests.cpp tools/smoketest/excel_tests.cpp tools/smoketest/template_fidelity.cpp tools/smoketest/recover_tests.cpp tools/smoketest/api_tests.cpp)
  target_link_libraries(dung-smoketest PRIVATE dungengine)
  target_include_directories(dung-smoketest PRIVATE third_party/libreofficekit/include app/src/excel)
  if(WIN32)
    target_link_libraries(dung-smoketest PRIVATE shell32)
  endif()
  # Thăm dò: gõ chữ rồi kiểm tra engine có vẽ lại tile không (Writer, Calc, Impress)
  add_executable(dung-repaintprobe tools/probe/repaint.cpp)
  target_link_libraries(dung-repaintprobe PRIVATE dungengine)
  target_include_directories(dung-repaintprobe PRIVATE third_party/libreofficekit/include)
endif()

if(DUNG_BUILD_APP)
  add_subdirectory(app)
endif()

# Công cụ Word (nguồn thêm vào dung-smoketest và DungOffice)
include(app/src/word/word.cmake)
