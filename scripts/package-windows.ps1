<#
.SYNOPSIS  Gom ứng dụng + Qt + runtime engine thành thư mục chạy được (dist\DungOffice).
#>
param(
  [Parameter(Mandatory = $true)][string]$BuildDir,
  [Parameter(Mandatory = $true)][string]$EngineDir,
  [Parameter(Mandatory = $true)][string]$QtDir,
  [Parameter(Mandatory = $true)][string]$OutDir,
  [string]$QpdfDir = "",
  # Thư mục chứa từ điển tiếng Việt đã tải (scripts\fetch-dict-vi.ps1). Từ điển là thành phần GPL RIÊNG:
  # được dựng ở <OutDir>\dict-vi (ngoài thư mục ứng dụng) để bộ cài đóng thành thành phần tuỳ chọn.
  [string]$DictViDir = ""
)
$ErrorActionPreference = "Stop"
$app = Join-Path $OutDir "DungOffice"
if (Test-Path $app) { Remove-Item -Recurse -Force $app }
New-Item -ItemType Directory -Force -Path $app | Out-Null

# Đích của target DungOffice nằm trong thư mục con app\ của thư mục build (xem app/CMakeLists.txt)
$exe = Join-Path $BuildDir "app\DungOffice.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $BuildDir "DungOffice.exe" }
if (-not (Test-Path $exe)) { throw "Không tìm thấy DungOffice.exe trong $BuildDir" }
Copy-Item $exe $app
& (Join-Path $QtDir "bin\windeployqt.exe") --release --no-translations --no-system-d3d-compiler `
   --no-opengl-sw --no-compiler-runtime (Join-Path $app "DungOffice.exe")
if ($LASTEXITCODE) { throw "windeployqt lỗi" }

# C++ runtime đặt cạnh ứng dụng (không cần cài VC++ Redistributable, không cần quyền admin)
if ($env:VCToolsRedistDir) {
  $crt = Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
  if ($crt) { Copy-Item (Join-Path $crt.FullName "*.dll") $app }
}

Write-Host "Sao chép engine…"
Copy-Item -Recurse $EngineDir (Join-Path $app "engine")
if ($crt) {
  foreach ($d in Get-ChildItem (Join-Path $crt.FullName "*.dll")) {
    $dst = Join-Path $app "engine\program\$($d.Name)"
    if (-not (Test-Path $dst)) { Copy-Item $d.FullName $dst }
  }
}

# qpdf (Apache-2.0) cho Công cụ PDF: đặt trong qpdf\ cạnh DungOffice.exe
if ($QpdfDir -and (Test-Path (Join-Path $QpdfDir "qpdf.exe"))) {
  Copy-Item -Recurse $QpdfDir (Join-Path $app "qpdf")
} else { Write-Warning "Không có qpdf: Công cụ PDF sẽ báo thiếu thành phần." }

$root = Resolve-Path "$PSScriptRoot\.."

# --- Từ điển tiếng Việt (GPL): KHÔNG để lẫn trong engine/ứng dụng ---
# 1) Gỡ mọi từ điển tiếng Việt nếu bản engine có sẵn (tránh phân phối dữ liệu GPL không tách bạch trong thư mục engine).
$removed = @()
$engineDir = Join-Path $app "engine"
foreach ($d in @(Get-ChildItem $engineDir -Recurse -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "dict-vi*" })) {
  $removed += $d.FullName.Substring($app.Length + 1); Remove-Item -Recurse -Force $d.FullName
}
foreach ($f in @(Get-ChildItem $engineDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(vi[_-][A-Za-z]*|vi)\.(dic|aff)$' })) {
  $removed += $f.FullName.Substring($app.Length + 1); Remove-Item -Force $f.FullName
}
$dictOut = Join-Path $OutDir "dict-vi"
if (Test-Path $dictOut) { Remove-Item -Recurse -Force $dictOut }
if ($DictViDir -and (Test-Path (Join-Path $DictViDir "vi-DauMoi.dic"))) {
  New-Item -ItemType Directory -Force -Path $dictOut | Out-Null
  Copy-Item (Join-Path $DictViDir "*") $dictOut
  Copy-Item (Join-Path $root "third_party\dict-vi\*") $dictOut   # GPL-2.0.txt, GPL-3.0.txt, NOTICE-vi.txt
  Write-Host "Từ điển tiếng Việt (thành phần riêng): $dictOut"
} else { Write-Warning "Không có từ điển tiếng Việt: bộ cài sẽ không có thành phần này." }
$removed | Set-Content -Encoding UTF8 (Join-Path $OutDir "dict-vi-removed-from-engine.txt")
Write-Host ("Từ điển tiếng Việt gỡ khỏi engine: {0} mục" -f $removed.Count)
# Gói mẫu đi kèm: ứng dụng tự cài vào thư mục dữ liệu người dùng khi chạy lần đầu (hoặc khi gói mới hơn)
if (Test-Path (Join-Path $root "templates")) { Copy-Item -Recurse (Join-Path $root "templates") (Join-Path $app "templates") }
Copy-Item (Join-Path $root "LICENSE") $app
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") $app
$size = (Get-ChildItem $app -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("Thư mục chạy được: {0}  ({1:N0} MB)" -f $app, $size)
