# Công cụ PDF: nguồn bổ sung cho DungOffice và bộ kiểm thử dung-pdftest (chỉ cần QtCore + qpdf).
set(DUNG_PDF_DIR ${CMAKE_CURRENT_LIST_DIR})
target_sources(DungOffice PRIVATE
  ${DUNG_PDF_DIR}/PdfOps.h ${DUNG_PDF_DIR}/PdfOps.cpp
  ${DUNG_PDF_DIR}/PdfOcr.h ${DUNG_PDF_DIR}/PdfOcr.cpp
  ${DUNG_PDF_DIR}/PdfToolsDialog.h ${DUNG_PDF_DIR}/PdfToolsDialog.cpp)
target_include_directories(DungOffice PRIVATE ${DUNG_PDF_DIR})

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui)
add_executable(dung-pdftest
  ${PROJECT_SOURCE_DIR}/tests/pdf/pdf_tests.cpp
  ${DUNG_PDF_DIR}/PdfOps.cpp ${DUNG_PDF_DIR}/PdfOps.h
  ${DUNG_PDF_DIR}/PdfOcr.cpp ${DUNG_PDF_DIR}/PdfOcr.h)
target_include_directories(dung-pdftest PRIVATE ${DUNG_PDF_DIR} ${PROJECT_SOURCE_DIR}/app/src)
target_link_libraries(dung-pdftest PRIVATE Qt6::Core Qt6::Gui)
set_target_properties(dung-pdftest PROPERTIES AUTOMOC OFF)

# Real OCR smoke: installed tesseract + traineddata, uses only local synthetic images.
add_executable(dung-ocrtest
  ${PROJECT_SOURCE_DIR}/tests/pdf/ocr_tests.cpp
  ${DUNG_PDF_DIR}/PdfOcr.cpp)
target_include_directories(dung-ocrtest PRIVATE ${DUNG_PDF_DIR})
target_link_libraries(dung-ocrtest PRIVATE Qt6::Core Qt6::Gui)
set_target_properties(dung-ocrtest PROPERTIES AUTOMOC OFF)
find_package(Qt6 6.4 QUIET OPTIONAL_COMPONENTS Pdf)
if(TARGET Qt6::Pdf)
  target_link_libraries(dung-ocrtest PRIVATE Qt6::Pdf)
  target_compile_definitions(dung-ocrtest PRIVATE DUNG_OCR_TEST_QTPDF=1)
endif()

# Optional standalone PDFium SDK; only use externally verified distributions.
set(DUNG_PDFIUM_ROOT "" CACHE PATH "Verified PDFium SDK root")
foreach(t DungOffice dung-pdftest)
  target_sources(${t} PRIVATE ${DUNG_PDF_DIR}/PdfTextEdit.cpp ${DUNG_PDF_DIR}/PdfTextEdit.h)
endforeach()
if(DUNG_PDFIUM_ROOT)
  find_path(DUNG_PDFIUM_INCLUDE fpdf_edit.h PATHS "${DUNG_PDFIUM_ROOT}/include" "${DUNG_PDFIUM_ROOT}/public" NO_DEFAULT_PATH REQUIRED)
  find_library(DUNG_PDFIUM_LIBRARY NAMES pdfium pdfium.dll PATHS "${DUNG_PDFIUM_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  foreach(t DungOffice dung-pdftest)
    target_include_directories(${t} PRIVATE "${DUNG_PDFIUM_INCLUDE}")
    target_link_libraries(${t} PRIVATE "${DUNG_PDFIUM_LIBRARY}")
    target_compile_definitions(${t} PRIVATE DUNG_HAVE_PDFIUM_EDIT=1)
  endforeach()
  if(WIN32)
    find_file(DUNG_PDFIUM_DLL pdfium.dll PATHS "${DUNG_PDFIUM_ROOT}/bin" NO_DEFAULT_PATH REQUIRED)
    foreach(t DungOffice dung-pdftest)
      add_custom_command(TARGET ${t} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DUNG_PDFIUM_DLL}" "$<TARGET_FILE_DIR:${t}>/pdfium.dll")
    endforeach()
  endif()
endif()
