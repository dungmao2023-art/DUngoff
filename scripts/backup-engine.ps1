# SPDX-License-Identifier: MPL-2.0
# Back up the working engine archive, not the build directory or user data.
# Requires authenticated GitHub CLI for this repository and 7-Zip.
param(
    [Parameter(Mandatory)][string]$BackupRoot,
    [string]$Repo = "dungmao2023-art/DUngoff"
)
$ErrorActionPreference = "Stop"
$tag = "engine-26.8.3.1-headless"
$expected = "03261237878855c27013ff81ba03d766fd794ff49f88d711a013747f7325cac1"
$asset = "dung-engine-headless-win64.7z"
$root = [IO.Path]::GetFullPath($BackupRoot)
New-Item -ItemType Directory -Force -Path $root | Out-Null
$dest = Join-Path $root $tag
if (Test-Path -LiteralPath $dest) { throw "Backup already exists; not overwriting: $dest" }
$stage = Join-Path $root (".stage-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    gh release download $tag --repo $Repo --pattern $asset --dir $stage
    if ($LASTEXITCODE -ne 0) { throw "Release download failed" }
    $archive = Join-Path $stage $asset
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "Working engine SHA256 mismatch: $actual" }
    $seven = (Get-Command 7z -ErrorAction SilentlyContinue).Source
    if (-not $seven) { $seven = "C:\Program Files\7-Zip\7z.exe" }
    if (-not (Test-Path -LiteralPath $seven)) { throw "7-Zip is required for archive integrity verification" }
    & $seven t $archive
    if ($LASTEXITCODE -ne 0) { throw "Archive integrity check failed" }
    [ordered]@{ repository=$Repo; tag=$tag; asset=$asset; sha256=$actual; bytes=(Get-Item -LiteralPath $archive).Length; source="https://github.com/$Repo/releases/tag/$tag"; verifiedUtc=[DateTime]::UtcNow.ToString("o") } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage "manifest.json") -Encoding utf8
    # Directory.Move refuses an existing destination, including a concurrent backup.
    [IO.Directory]::Move($stage,$dest)
    Write-Host "Verified rollback archive: $dest"
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
