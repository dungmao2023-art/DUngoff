# Nhánh Word: nguồn của công cụ Word. Được include từ CMakeLists.txt gốc (sau add_subdirectory(app)).
set(_DUNG_WORD_DIR ${CMAKE_CURRENT_LIST_DIR})
if(TARGET dung-smoketest)
  target_sources(dung-smoketest PRIVATE
    ${_DUNG_WORD_DIR}/WordCommands.h ${_DUNG_WORD_DIR}/WordCommands.cpp
    ${CMAKE_SOURCE_DIR}/tools/smoketest/word_tests.cpp)
endif()
if(TARGET DungOffice)
  target_sources(DungOffice PRIVATE
    ${_DUNG_WORD_DIR}/WordCommands.h ${_DUNG_WORD_DIR}/WordCommands.cpp
    ${_DUNG_WORD_DIR}/WordRibbon.cpp
    ${_DUNG_WORD_DIR}/WordActions.h ${_DUNG_WORD_DIR}/WordActions.cpp
    ${_DUNG_WORD_DIR}/../vntext/VnText.h ${_DUNG_WORD_DIR}/../vntext/VnText.cpp
    ${_DUNG_WORD_DIR}/../vntext/SyllableCheck.h ${_DUNG_WORD_DIR}/../vntext/SyllableCheck.cpp)
endif()
