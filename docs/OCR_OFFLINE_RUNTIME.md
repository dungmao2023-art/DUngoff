# OCR offline Windows

`scripts/build-ocr-windows.py build/ocr-runtime --work build/vcpkg-ocr` builds Tesseract 5.5.1 separately from Office using vcpkg commit `74e6536215718009aae747d86d84b78376bf9e09`, release-only dynamic dependencies. English and Vietnamese tessdata_fast 4.1.0 and the upstream PDF text-layer font are pinned by Git blob and size. The bundle includes dependency notices and a SHA-256 manifest; the packaging script verifies the runtime again.

CI requires a real English/Vietnamese OCR and searchable PDF test using the local executable without an installed Tesseract in PATH. No network access is needed at app runtime. Windows packaging places the runtime in `tesseract/` next to DungOffice.exe.

Before public distribution: review all dependency licenses/source obligations and verify installed runtime on a clean Windows machine. The first build may be expensive; do not claim a completed installer before CI passes.
