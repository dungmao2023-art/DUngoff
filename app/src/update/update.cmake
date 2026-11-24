# Kiểm tra cập nhật có xác thực SHA-256 + một phiên duy nhất (nhấp đúp tệp). Bộ kiểm thử: dung-updatetest.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network)
set(DUNG_UPD_DIR ${CMAKE_CURRENT_LIST_DIR})
target_sources(DungOffice PRIVATE
  ${DUNG_UPD_DIR}/UpdateCore.h ${DUNG_UPD_DIR}/UpdateCore.cpp
  ${DUNG_UPD_DIR}/UpdateChecker.h ${DUNG_UPD_DIR}/UpdateChecker.cpp
  ${DUNG_UPD_DIR}/UpdateDialog.h ${DUNG_UPD_DIR}/UpdateDialog.cpp
  ${DUNG_UPD_DIR}/../SingleInstance.h ${DUNG_UPD_DIR}/../SingleInstance.cpp)
target_link_libraries(DungOffice PRIVATE Qt6::Network)
target_include_directories(DungOffice PRIVATE ${DUNG_UPD_DIR})

add_executable(dung-updatetest
  ${PROJECT_SOURCE_DIR}/tests/update/update_tests.cpp
  ${DUNG_UPD_DIR}/UpdateCore.h ${DUNG_UPD_DIR}/UpdateCore.cpp
  ${DUNG_UPD_DIR}/../SingleInstance.h ${DUNG_UPD_DIR}/../SingleInstance.cpp)
target_include_directories(dung-updatetest PRIVATE ${DUNG_UPD_DIR} ${DUNG_UPD_DIR}/..)
target_link_libraries(dung-updatetest PRIVATE Qt6::Core Qt6::Network)
if(WIN32)
  target_link_libraries(DungOffice PRIVATE user32)
  target_link_libraries(dung-updatetest PRIVATE user32)
endif()
