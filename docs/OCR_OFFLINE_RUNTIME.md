# Gói OCR offline tùy chọn

Tesseract upstream: https://github.com/tesseract-ocr/tesseract (Apache-2.0). Dữ liệu ngôn ngữ: https://github.com/tesseract-ocr/tessdata . Phải xác minh license, checksum và phiên bản thực tế của từng binary, traineddata, pdf.ttf và DLL phụ thuộc trước khi phân phối.

Bố cục dự kiến cạnh DungOffice.exe:

```
tesseract/
  tesseract.exe
  [runtime DLL dependencies]
  tessdata/
    eng.traineddata
    vie.traineddata
    pdf.ttf
  [LICENSE / NOTICE / dependency notices]
```

Linux dùng tên tesseract không đuôi exe. Ưu tiên runtime cố định theo thư mục ứng dụng, truyền --tessdata-dir rõ ràng; nếu không có thì dùng Tesseract trong PATH. Không tự tải, không shell, không gửi ảnh ra mạng. OCR bản sao PDF cần font pdf.ttf. Chưa đóng gói bất kỳ binary hoặc model mới nào ở PR này; cần nghiệm thu installer sạch không có PATH Tesseract, OCR vie/eng và PDF searchable thực tế trước khi đưa vào bộ cài.
