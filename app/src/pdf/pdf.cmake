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
