<#
.SYNOPSIS
  Tải bản LibreOffice chính thức (MSI Windows x64) từ máy chủ của The Document Foundation,
  kiểm tra SHA-256, rồi giải nén (administrative install, không cài vào hệ thống) để làm
  runtime engine cho Dũng Office.

.PARAMETER Version   Phiên bản LibreOffice, ví dụ 26.8.3
.PARAMETER OutDir    Thư mục đích; runtime nằm ở <OutDir>\engine (chứa program\sofficeapp.dll)

Chỉ dùng trên máy build / CI. Người dùng cuối nhận runtime đã đóng gói sẵn trong bộ cài.
#>
param(
  [string]$Version = "26.8.3",
  [string]$OutDir = "$PSScriptRoot\..\build\lo",
  [switch]$SkipHashCheck
)
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$msiName = "LibreOffice_${Version}_Win_x86-64.msi"
$msi = Join-Path $OutDir $msiName
$urls = @(
  "https://download.documentfoundation.org/libreoffice/stable/$Version/win/x86_64/$msiName",
  "https://downloadarchive.documentfoundation.org/libreoffice/old/$Version.1/win/x86_64/$msiName",
  "https://downloadarchive.documentfoundation.org/libreoffice/old/$Version.2/win/x86_64/$msiName"
)

if (-not (Test-Path $msi)) {
  $ok = $false
  foreach ($u in $urls) {
    try {
      Write-Host "Tải $u"
      Invoke-WebRequest -Uri $u -OutFile $msi -UseBasicParsing
      $script:srcUrl = $u
      $ok = $true
      break
    } catch { Write-Host "  không tải được: $($_.Exception.Message)" }
  }
  if (-not $ok) { throw "Không tải được LibreOffice $Version" }
} else { $script:srcUrl = $urls[0] }

if (-not $SkipHashCheck) {
  # MirrorBrain của TDF cung cấp tổng kiểm tra ở <url>.sha256
  $expected = $null
  foreach ($u in @("$($script:srcUrl).sha256") + ($urls | ForEach-Object { "$_.sha256" })) {
    try {
      $txt = (Invoke-WebRequest -Uri $u -UseBasicParsing).Content
      if ($txt -is [byte[]]) { $txt = [System.Text.Encoding]::ASCII.GetString($txt) }
      if ($txt -match '([0-9a-fA-F]{64})') { $expected = $Matches[1].ToLower(); break }
    } catch { }
  }
  if (-not $expected) { throw "Không lấy được SHA-256 chính thức để kiểm tra. Dùng -SkipHashCheck nếu chắc chắn." }
  $actual = (Get-FileHash $msi -Algorithm SHA256).Hash.ToLower()
  if ($actual -ne $expected) { Remove-Item $msi; throw "SHA-256 không khớp! mong đợi $expected, thực tế $actual" }
  Write-Host "SHA-256 khớp: $actual"
}

$admin = Join-Path $OutDir "admin"
if (Test-Path $admin) { Remove-Item -Recurse -Force $admin }
Write-Host "Giải nén MSI (administrative install)…"
$p = Start-Process msiexec.exe -ArgumentList @("/a", "`"$msi`"", "/qn", "TARGETDIR=`"$admin`"") -Wait -PassThru
if ($p.ExitCode -ne 0) { throw "msiexec /a lỗi $($p.ExitCode)" }

$dll = Get-ChildItem -Path $admin -Recurse -Filter sofficeapp.dll | Select-Object -First 1
if (-not $dll) { throw "Không tìm thấy sofficeapp.dll sau khi giải nén" }
$root = Split-Path (Split-Path $dll.FullName -Parent) -Parent   # thư mục chứa program\
$engine = Join-Path $OutDir "engine"
if (Test-Path $engine) { Remove-Item -Recurse -Force $engine }
Move-Item $root $engine
Remove-Item -Recurse -Force $admin

# Chưa lược bớt tệp nào: mọi việc thu gọn runtime phải đi kèm kiểm thử chứng minh không ảnh hưởng
# đến xử lý tài liệu (xem docs/ROADMAP.md, giai đoạn "Engine gọn").
Set-Content -Path (Join-Path $engine "DUNG_ENGINE_SOURCE.txt") -Encoding UTF8 -Value @"
Runtime engine của Dũng Office = LibreOffice $Version (bản phát hành chính thức, Windows x64)
Nguồn: $($script:srcUrl)
Giấy phép: MPL-2.0 (xem thư mục readmes/ và license/ bên trong)
Mã nguồn tương ứng: https://download.documentfoundation.org/libreoffice/src/
"@
Write-Host "Engine sẵn sàng: $engine\program"
"$engine\program"
