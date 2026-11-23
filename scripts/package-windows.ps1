<#
.SYNOPSIS  Gom ứng dụng + Qt + runtime engine thành thư mục chạy được (dist\DungOffice).
#>
param(
  [Parameter(Mandatory = $true)][string]$BuildDir,
  [Parameter(Mandatory = $true)][string]$EngineDir,
  [Parameter(Mandatory = $true)][string]$QtDir,
  [Parameter(Mandatory = $true)][string]$OutDir
)
$ErrorActionPreference = "Stop"
$app = Join-Path $OutDir "DungOffice"
if (Test-Path $app) { Remove-Item -Recurse -Force $app }
New-Item -ItemType Directory -Force -Path $app | Out-Null

Copy-Item (Join-Path $BuildDir "DungOffice.exe") $app
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

$root = Resolve-Path "$PSScriptRoot\.."
Copy-Item (Join-Path $root "LICENSE") $app
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") $app
$size = (Get-ChildItem $app -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("Thư mục chạy được: {0}  ({1:N0} MB)" -f $app, $size)
