<#
.SYNOPSIS  Build Dũng Office trên Windows (dành cho nhà phát triển).
  Yêu cầu: Visual Studio 2022 (C++), CMake, Ninja, Qt 6.8 (msvc2022_64), Python 3 + openpyxl (chỉ để kiểm thử).
  Chạy trong "x64 Native Tools Command Prompt for VS 2022" / Developer PowerShell:
     .\scripts\build-windows.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
#>
param(
  [Parameter(Mandatory = $true)][string]$QtDir,
  [string]$LoVersion = "26.8.3",
  [switch]$SkipTests
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\.."
$build = Join-Path $root "build"
$prog = (& "$PSScriptRoot\fetch-libreoffice.ps1" -Version $LoVersion -OutDir "$build\lo" | Select-Object -Last 1)

cmake -S $root -B "$build\out" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtDir"
if ($LASTEXITCODE) { throw "cmake configure lỗi" }
cmake --build "$build\out" --parallel
if ($LASTEXITCODE) { throw "build lỗi" }

if (-not $SkipTests) {
  python "$root\tests\fixtures\make_fixtures.py" "$build\fixtures"
  & "$build\out\dung-smoketest.exe" --program $prog --xlsx "$build\fixtures\mau-doanh-thu.xlsx" --csv "$build\fixtures\mau.csv" `
    --docx "$build\fixtures\mau-van-ban.docx" --pptx "$build\fixtures\mau-trinh-chieu.pptx" --out "$build\smoke"
  if ($LASTEXITCODE) { throw "smoke test lỗi" }
  $saved = Get-Content "$build\smoke\saved-path.txt" -Encoding UTF8
  python "$root\tests\verify_roundtrip.py" "$build\fixtures\mau-doanh-thu.xlsx" $saved `
    --expect "Doanh thu!F2=Dũng" --expect "Doanh thu!F3==D5*2" --expect 'Doanh thu!G2=="Tổng: "&D5' `
    --known "$root\tests\known_differences.txt"
  if ($LASTEXITCODE) { throw "kiểm tra round-trip lỗi" }
}

& "$PSScriptRoot\package-windows.ps1" -BuildDir "$build\out" -EngineDir "$build\lo\engine" -QtDir $QtDir -OutDir "$build\dist"
