# Bản phục hồi (tự lưu): kho QtCore + hộp thoại; bộ kiểm thử dung-recoverytest.
set(DUNG_REC_DIR ${CMAKE_CURRENT_LIST_DIR})
target_sources(DungOffice PRIVATE
  ${DUNG_REC_DIR}/RecoveryStore.h ${DUNG_REC_DIR}/RecoveryStore.cpp
  ${DUNG_REC_DIR}/RecoveryDialog.h ${DUNG_REC_DIR}/RecoveryDialog.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_REC_DIR})
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_executable(dung-recoverytest
  ${PROJECT_SOURCE_DIR}/tests/recovery/recovery_tests.cpp
  ${DUNG_REC_DIR}/RecoveryStore.cpp ${DUNG_REC_DIR}/RecoveryStore.h
  ${DUNG_REC_DIR}/../extras/WorkspaceStore.cpp ${DUNG_REC_DIR}/../extras/WorkspaceStore.h)
target_include_directories(dung-recoverytest PRIVATE ${DUNG_REC_DIR})
target_link_libraries(dung-recoverytest PRIVATE Qt6::Core)
set_target_properties(dung-recoverytest PROPERTIES AUTOMOC OFF)
