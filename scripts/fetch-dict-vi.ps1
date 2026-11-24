<#
.SYNOPSIS
  Tải từ điển Hunspell tiếng Việt (GPL) từ một commit đã ghim của https://github.com/1ec5/hunspell-vi,
  kiểm tra SHA-256 từng tệp, rồi đặt vào <OutDir> (thành phần tuỳ chọn, tách khỏi ứng dụng và engine).
  Tệp không bị sửa. Văn bản giấy phép/thông báo bản quyền gốc được tải kèm.
#>
param([string]$OutDir = "$PSScriptRoot\..\build\dict-vi")
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$commit = "39cc647f0cf3bdfc579980a2c3aac3f6f4203fe6"
$base = "https://raw.githubusercontent.com/1ec5/hunspell-vi/$commit"
# đường dẫn trong kho -> (tên lưu, SHA-256)
$files = @(
  @("dictionaries/vi-DauCu.dic",  "vi-DauCu.dic",  "50b23ff20ea83ad886e2ff166220d53b307784c9870fb447341e0f298f0e5588"),
  @("dictionaries/vi-DauCu.aff",  "vi-DauCu.aff",  "0d836ebf1d4f1e9964d931fbb76464b24037b71995c73784166a07c43e2a7ae5"),
  @("dictionaries/vi-DauMoi.dic", "vi-DauMoi.dic", "a075773a27729713a1f5853404c7aafbb78e9056a12ccd729c19622953723143"),
  @("dictionaries/vi-DauMoi.aff", "vi-DauMoi.aff", "0d836ebf1d4f1e9964d931fbb76464b24037b71995c73784166a07c43e2a7ae5"),
  @("dictionaries/README-en.txt", "README-en.txt", "3520a6e35cc2e839d29193daeeccf9a146288732f7022678c5b44fcae9cd4a0e"),
  @("dictionaries/README-vi.txt", "README-vi.txt", "b4e0b0e0a0607c529a09487ee3f23d86476e2278131cd8386258e6a66f028d16")
)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
foreach ($f in $files) {
  $dst = Join-Path $OutDir $f[1]
  Invoke-WebRequest -Uri "$base/$($f[0])" -OutFile $dst -UseBasicParsing
  $h = (Get-FileHash $dst -Algorithm SHA256).Hash.ToLower()
  if ($h -ne $f[2]) { Remove-Item $dst; throw "SHA-256 không khớp cho $($f[0]): mong đợi $($f[2]), thực tế $h" }
  Write-Host "OK  $($f[1])  $h"
}
Write-Host "Đã tải từ điển tiếng Việt vào $OutDir (commit $commit)"
