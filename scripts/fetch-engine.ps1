<#
.SYNOPSIS
  Tải engine headless do chính dự án này build (Release của repo), kiểm tra SHA-256 đã ghim,
  rồi giải nén vào <OutDir>\engine (chứa program\mergedlo.dll và dung-engine-headless.txt).

.PARAMETER Tag      Tag Release, ví dụ engine-26.8.3.1-headless
.PARAMETER Sha256   SHA-256 mong đợi của dung-engine-headless-win64.7z (ghim trong CI)
.PARAMETER Repo     owner/repo chứa Release
#>
param(
  [Parameter(Mandatory)][string]$Tag,
  [Parameter(Mandatory)][string]$Sha256,
  [string]$Repo = "dungmao2023-art/DUngoff",
  [string]$OutDir = "$PSScriptRoot\..\build\lo"
)
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$archive = Join-Path $OutDir "dung-engine-headless-win64.7z"
$url = "https://github.com/$Repo/releases/download/$Tag/dung-engine-headless-win64.7z"

Write-Host "Tải $url"
Invoke-WebRequest -Uri $url -OutFile $archive -UseBasicParsing
$actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $Sha256.ToLower()) {
  Remove-Item $archive
  throw "SHA-256 không khớp! mong đợi $Sha256, thực tế $actual"
}
Write-Host "SHA-256 khớp: $actual"

$engine = Join-Path $OutDir "engine"
if (Test-Path $engine) { Remove-Item -Recurse -Force $engine }
$sevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source
if (-not $sevenZip) { $sevenZip = "C:\Program Files\7-Zip\7z.exe" }
if (-not (Test-Path $sevenZip)) { throw "Không tìm thấy 7z để giải nén" }
& $sevenZip x -y "-o$OutDir" $archive | Select-Object -Last 4
if ($LASTEXITCODE -ne 0) { throw "7z giải nén lỗi $LASTEXITCODE" }

if (-not (Test-Path (Join-Path $engine "program\mergedlo.dll"))) { throw "Thiếu program\mergedlo.dll sau khi giải nén" }
if (-not (Test-Path (Join-Path $engine "dung-engine-headless.txt"))) { throw "Thiếu dung-engine-headless.txt: đây không phải engine headless" }
Write-Host "Engine sẵn sàng: $engine"
Get-Content (Join-Path $engine "dung-engine-headless.txt")
