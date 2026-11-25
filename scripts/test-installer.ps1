# Kiểm bộ cài Windows từ đầu đến cuối (chạy im lặng trên CI): cài -> kiểm file/registry/lối tắt -> chạy app đã cài -> gỡ -> kiểm sạch.
# Không kiểm được: giao diện thật của trình cài (trang chọn thành phần, giấy phép, nút bấm) - chỉ chạy chế độ im lặng.
param(
  [Parameter(Mandatory)] [string]$Setup,      # đường dẫn DungOffice-Setup-*.exe
  [Parameter(Mandatory)] [string]$DistApp,    # build\dist\DungOffice (nguồn đã đóng gói)
  [string]$DistDict = "",                     # build\dist\dict-vi
  [Parameter(Mandatory)] [string]$Docx,       # tệp .docx mẫu để mở thử
  [Parameter(Mandatory)] [string]$Report      # thư mục ghi nhật ký
)
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force -Path $Report | Out-Null
$log = Join-Path $Report "installer-e2e.log"
"" | Set-Content $log -Encoding UTF8
$script:fails = 0
function Say($m) { Write-Host $m; Add-Content $log $m -Encoding UTF8 }
function Ok($c, $m) { if ($c) { Say "[ĐẠT] $m" } else { Say "[LỖI] $m"; $script:fails++ } }
function Note($m) { Say "[GHI CHÚ] $m" }

$appId = "{6C1E7B53-2F7B-4C4B-9C59-0D0A1B2C3D4E}_is1"
$uninstKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$appId"
$exts = "docx", "xlsx", "pptx", "pdf"
$startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Dũng Office\Dũng Office.lnk"
$desktopLnk = Join-Path ([Environment]::GetFolderPath("Desktop")) "Dũng Office.lnk"

function Rel($root, $p) { $p.Substring($root.TrimEnd('\').Length).TrimStart('\') }
function FileSet($root) {
  if (-not (Test-Path -LiteralPath $root)) { return @() }
  @(Get-ChildItem -LiteralPath $root -Recurse -File -Force | ForEach-Object { (Rel $root $_.FullName).ToLowerInvariant() })
}
function RegKeys($path) {
  if (-not (Test-Path $path)) { return @() }
  @(Get-ChildItem $path -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $_.Name }) | Sort-Object
}
function Snapshot {
  @((RegKeys "HKCU:\Software\Classes") + (RegKeys "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall")) | Sort-Object
}
function LnkTarget($lnk) {
  try { (New-Object -ComObject WScript.Shell).CreateShortcut($lnk).TargetPath } catch { $null }
}
function RegDefault($path) { try { (Get-Item $path -ErrorAction Stop).GetValue("") } catch { $null } }

function Install($tag, $components, $tasks, $dir) {
  $logFile = Join-Path $Report "setup-$tag.log"
  $sargs = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LOG=`"$logFile`"", "/COMPONENTS=`"$components`"", "/TASKS=`"$tasks`"")
  if ($dir) { $sargs += "/DIR=`"$dir`"" }
  Say "== Cài [$tag]: $($sargs -join ' ')"
  $p = Start-Process $Setup -ArgumentList $sargs -PassThru -Wait
  Ok ($p.ExitCode -eq 0) "[$tag] bộ cài thoát mã 0 (thực tế $($p.ExitCode))"
  if ($p.ExitCode -ne 0 -and (Test-Path $logFile)) { Get-Content $logFile -Tail 40 | ForEach-Object { Say "    $_" } }
}

function InstallDirFromRegistry {
  try { (Get-ItemProperty $uninstKey -ErrorAction Stop).InstallLocation } catch { $null }
}

function RunInstalledApp($tag, $exe) {
  Say "== Chạy app đã cài [$tag] với tệp .docx mẫu"
  $p = Start-Process $exe -ArgumentList "`"$Docx`"" -PassThru
  Start-Sleep 20
  Ok (-not $p.HasExited) "[$tag] app vẫn chạy sau 20 giây (nếu thiếu DLL sẽ thoát ngay; mã thoát nếu chết: $(if ($p.HasExited) { '0x{0:X}' -f $p.ExitCode } else { '-' }))"
  if (-not $p.HasExited) {
    try {
      $mods = @($p.Modules | ForEach-Object { $_.FileName })
      $inDir = @($mods | Where-Object { $_ -like (Split-Path $exe -Parent) + "*" })
      Ok ($inDir.Count -ge 5) "[$tag] app nạp $($inDir.Count) mô-đun từ thư mục cài (Qt/engine nằm trong thư mục cài)"
      Ok ((@($mods | Where-Object { $_ -match '(?i)\\Qt6Core\.dll$' }).Count) -ge 1) "[$tag] Qt6Core.dll được nạp"
    } catch { Note "[$tag] không liệt kê được mô-đun: $($_.Exception.Message)" }
    $null = $p.CloseMainWindow()
    if (-not $p.WaitForExit(30000)) {
      Note "[$tag] app không tự thoát sau CloseMainWindow 30 giây (có thể đang hiện hộp thoại hỏi) - buộc dừng"
      Stop-Process $p -Force
      $script:fails++
      Say "[LỖI] [$tag] thoát không sạch"
    } else {
      Ok ($p.ExitCode -eq 0) "[$tag] app thoát sạch, mã $($p.ExitCode)"
    }
  }
}

function Uninstall($tag, $dir) {
  $unins = Join-Path $dir "unins000.exe"
  Ok (Test-Path -LiteralPath $unins) "[$tag] có unins000.exe"
  if (-not (Test-Path -LiteralPath $unins)) { return }
  Say "== Gỡ [$tag]"
  $p = Start-Process $unins -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/LOG=`"$(Join-Path $Report "unins-$tag.log")`"" -PassThru -Wait
  # Trình gỡ của Inno có thể trả về trước khi bản sao tạm làm xong: đợi tới 120 giây
  $t = 0
  while ((Test-Path $uninstKey) -and $t -lt 120) { Start-Sleep 2; $t += 2 }
  Start-Sleep 3
  Ok ($p.ExitCode -eq 0) "[$tag] trình gỡ thoát mã 0 (thực tế $($p.ExitCode))"
}

# ------------------------------------------------------------------------------------------------
Say "Bộ cài: $Setup ($([math]::Round((Get-Item $Setup).Length/1MB)) MB)"
$srcFiles = FileSet $DistApp
$dictFiles = if ($DistDict -and (Test-Path $DistDict)) { FileSet $DistDict } else { @() }
Say "Nguồn: $($srcFiles.Count) tệp ứng dụng, $($dictFiles.Count) tệp từ điển"
Ok ($srcFiles.Count -gt 100) "nguồn đóng gói có đủ tệp"

# Một người dùng đã có phần mềm khác mở .pdf: bộ cài không được đè
$pdfKey = "HKCU:\Software\Classes\.pdf"
New-Item -Force -Path $pdfKey | Out-Null
Set-ItemProperty -Path $pdfKey -Name "(default)" -Value "OtherApp.pdf"

$before = Snapshot

# ============================== Kịch bản 1: đầy đủ, thư mục mặc định, mọi liên kết ==============================
$tasksAll = "desktopicon,startmenu,assoc_docx,assoc_xlsx,assoc_pptx,assoc_pdf"
Install "full" "main,dict_vi" $tasksAll $null
$dir1 = InstallDirFromRegistry
Ok ([bool]$dir1) "[full] mục gỡ cài đặt có InstallLocation: $dir1"
if (-not $dir1) { $dir1 = Join-Path $env:LOCALAPPDATA "Programs\DungOffice" }
$dir1 = $dir1.TrimEnd('\')
Ok ($dir1 -like "$env:LOCALAPPDATA\Programs\DungOffice") "[full] cài vào thư mục mặc định ($dir1)"
Ok (Test-Path -LiteralPath "$dir1\DungOffice.exe") "[full] có DungOffice.exe"
$inst = FileSet $dir1
$missing = @($srcFiles | Where-Object { $inst -notcontains $_ })
Ok ($missing.Count -eq 0) "[full] đủ mọi tệp ứng dụng (thiếu $($missing.Count))"
$missing | Select-Object -First 10 | ForEach-Object { Say "    thiếu: $_" }
foreach ($pat in "engine\program\*.dll", "qpdf\*") { Ok ($inst | Where-Object { $_ -like $pat.ToLower() }) "[full] có $pat" }
Ok ($inst | Where-Object { $_ -match '^qt6core\.dll$' }) "[full] có Qt6Core.dll"
$dictInst = @($inst | Where-Object { $_ -like "dictionaries\vi\*" })
$dictMissing = @($dictFiles | Where-Object { $dictInst -notcontains ("dictionaries\vi\" + $_) })
if ($dictFiles.Count) {
  Ok ($dictMissing.Count -eq 0 -and $dictInst.Count -eq $dictFiles.Count) "[full] dictionaries\vi có $($dictInst.Count)/$($dictFiles.Count) tệp, gồm giấy phép GPL"
  Ok ($dictInst -contains "dictionaries\vi\gpl-2.0.txt" -and $dictInst -contains "dictionaries\vi\gpl-3.0.txt") "[full] có GPL-2.0.txt và GPL-3.0.txt"
} else { Note "[full] không có thư mục từ điển nguồn (tải từ điển lỗi?): bỏ qua kiểm tra dictionaries\vi" }
$appDictBad = @($inst | Where-Object { $_ -notlike "dictionaries\vi\*" -and $_ -match '(^|\\)vi[_-][^\\]*\.(dic|aff)$|(^|\\)vi\.(dic|aff)$' })
Ok ($appDictBad.Count -eq 0) "[full] không có tệp từ điển vi nằm ngoài dictionaries\vi"

# Registry
$ud = Get-ItemProperty $uninstKey -ErrorAction SilentlyContinue
Ok ($ud -and $ud.DisplayName -like "Dũng Office*") "[full] mục gỡ cài: DisplayName = $($ud.DisplayName)"
Ok ($ud -and $ud.UninstallString) "[full] mục gỡ cài có UninstallString: $($ud.UninstallString)"
$cmd = RegDefault "HKCU:\Software\Classes\Applications\DungOffice.exe\shell\open\command"
Ok ($cmd -and $cmd -like "*$dir1\DungOffice.exe*") "[full] Applications\DungOffice.exe\shell\open\command = $cmd"
foreach ($e in $exts) {
  Ok (Test-Path "HKCU:\Software\Classes\.$e\OpenWithList\DungOffice.exe") "[full] .$e\OpenWithList\DungOffice.exe"
  $c = RegDefault "HKCU:\Software\Classes\DungOffice.$e\shell\open\command"
  Ok ($c -and $c -like "*$dir1\DungOffice.exe*%1*") "[full] ProgID DungOffice.$e có lệnh mở: $c"
  $ow = (Get-Item "HKCU:\Software\Classes\.$e\OpenWithProgids" -ErrorAction SilentlyContinue)
  Ok ($ow -and ($ow.GetValueNames() -contains "DungOffice.$e")) "[full] .$e\OpenWithProgids có DungOffice.$e"
}
foreach ($e in "docx", "xlsx", "pptx") { Ok ((RegDefault "HKCU:\Software\Classes\.$e") -eq "DungOffice.$e") "[full] giá trị mặc định .$e = DungOffice.$e (đuôi chưa có phần mềm khác) -> $((RegDefault "HKCU:\Software\Classes\.$e"))" }
Ok ((RegDefault "HKCU:\Software\Classes\.pdf") -eq "OtherApp.pdf") "[full] .pdf đã có phần mềm khác: KHÔNG bị đè (giá trị = $((RegDefault "HKCU:\Software\Classes\.pdf")))"
# Lối tắt
Ok (Test-Path -LiteralPath $startMenu) "[full] lối tắt Start Menu"
Ok ((LnkTarget $startMenu) -eq "$dir1\DungOffice.exe") "[full] lối tắt Start Menu trỏ đúng: $(LnkTarget $startMenu)"
Ok (Test-Path -LiteralPath $desktopLnk) "[full] lối tắt màn hình nền"
Ok ((LnkTarget $desktopLnk) -eq "$dir1\DungOffice.exe") "[full] lối tắt màn hình nền trỏ đúng"

RunInstalledApp "full" "$dir1\DungOffice.exe"

# Gỡ
Uninstall "full" $dir1
Ok (-not (Test-Path $uninstKey)) "[full] mục gỡ cài đặt đã xoá"
$left = FileSet $dir1
Ok ($left.Count -eq 0) "[full] thư mục cài sạch sau khi gỡ (còn $($left.Count) tệp)"
$left | Select-Object -First 20 | ForEach-Object { Say "    còn lại: $_" }
Ok (-not (Test-Path -LiteralPath $startMenu)) "[full] lối tắt Start Menu đã xoá"
Ok (-not (Test-Path -LiteralPath $desktopLnk)) "[full] lối tắt màn hình nền đã xoá"
Ok (-not (Test-Path "HKCU:\Software\Classes\Applications\DungOffice.exe")) "[full] khoá Applications\DungOffice.exe đã xoá"
foreach ($e in $exts) {
  Ok (-not (Test-Path "HKCU:\Software\Classes\DungOffice.$e")) "[full] ProgID DungOffice.$e đã xoá"
  Ok (-not (Test-Path "HKCU:\Software\Classes\.$e\OpenWithList\DungOffice.exe")) "[full] .$e\OpenWithList đã xoá"
  $ow = (Get-Item "HKCU:\Software\Classes\.$e\OpenWithProgids" -ErrorAction SilentlyContinue)
  Ok (-not ($ow -and ($ow.GetValueNames() -contains "DungOffice.$e"))) "[full] .$e\OpenWithProgids sạch"
}
foreach ($e in "docx", "xlsx", "pptx") { Ok ((RegDefault "HKCU:\Software\Classes\.$e") -ne "DungOffice.$e") "[full] giá trị mặc định .$e đã gỡ" }
Ok ((RegDefault "HKCU:\Software\Classes\.pdf") -eq "OtherApp.pdf") "[full] .pdf vẫn là OtherApp.pdf sau khi gỡ"
Remove-Item -Recurse -Force "HKCU:\Software\Classes\.pdf" -ErrorAction SilentlyContinue
$afterRemove = Snapshot
$diff = @(Compare-Object $before $afterRemove | Where-Object { $_.SideIndicator -eq "=>" } | ForEach-Object { $_.InputObject })
Ok ($diff.Count -eq 0) "[full] không còn khoá registry nào do bộ cài tạo (thừa $($diff.Count))"
$diff | Select-Object -First 20 | ForEach-Object { Say "    thừa: $_" }

# ============ Kịch bản 2: tối thiểu (không từ điển), đường dẫn có dấu cách + tiếng Việt, chỉ liên kết .docx ============
$dir2 = Join-Path $env:RUNNER_TEMP "Thư mục có dấu cách\Dũng Office Cài đặt"
$before2 = Snapshot
Install "compact" "main" "startmenu,assoc_docx" $dir2
Ok (Test-Path -LiteralPath "$dir2\DungOffice.exe") "[compact] có DungOffice.exe trong đường dẫn có dấu cách và tiếng Việt: $dir2"
$inst2 = FileSet $dir2
$missing2 = @($srcFiles | Where-Object { $inst2 -notcontains $_ })
Ok ($missing2.Count -eq 0) "[compact] đủ mọi tệp ứng dụng (thiếu $($missing2.Count))"
Ok (-not (Test-Path -LiteralPath "$dir2\dictionaries\vi")) "[compact] KHÔNG có thư mục dictionaries\vi khi không chọn từ điển"
Ok ((@($inst2 | Where-Object { $_ -match '(^|\\)vi[_-][^\\]*\.(dic|aff)$|(^|\\)vi\.(dic|aff)$' })).Count -eq 0) "[compact] không có tệp từ điển vi nào"
$cmd2 = RegDefault "HKCU:\Software\Classes\DungOffice.docx\shell\open\command"
Ok ($cmd2 -eq "`"$dir2\DungOffice.exe`" `"%1`"") "[compact] lệnh mở .docx đúng và có ngoặc kép quanh đường dẫn: $cmd2"
Ok (-not (Test-Path "HKCU:\Software\Classes\DungOffice.xlsx")) "[compact] không tạo liên kết .xlsx khi không chọn"
Ok (-not (Test-Path -LiteralPath $desktopLnk)) "[compact] không có lối tắt màn hình nền khi không chọn"
Ok ((LnkTarget $startMenu) -eq "$dir2\DungOffice.exe") "[compact] lối tắt Start Menu trỏ đúng đường dẫn có dấu: $(LnkTarget $startMenu)"
# Mô phỏng Explorer: chạy đúng lệnh trong registry với tệp .docx
if ($cmd2) {
  $exe2 = "$dir2\DungOffice.exe"
  RunInstalledApp "compact" $exe2
}
Uninstall "compact" $dir2
Ok (-not (Test-Path $uninstKey)) "[compact] mục gỡ cài đặt đã xoá"
$left2 = FileSet $dir2
Ok ($left2.Count -eq 0) "[compact] thư mục cài sạch sau khi gỡ (còn $($left2.Count) tệp)"
$left2 | Select-Object -First 20 | ForEach-Object { Say "    còn lại: $_" }
Ok (-not (Test-Path -LiteralPath $startMenu)) "[compact] lối tắt Start Menu đã xoá"
$diff2 = @(Compare-Object $before2 (Snapshot) | Where-Object { $_.SideIndicator -eq "=>" } | ForEach-Object { $_.InputObject })
Ok ($diff2.Count -eq 0) "[compact] không còn khoá registry thừa ($($diff2.Count))"
$diff2 | Select-Object -First 20 | ForEach-Object { Say "    thừa: $_" }

# Dữ liệu người dùng cố ý để lại (cài đặt, bản phục hồi...)
foreach ($d in (Join-Path $env:APPDATA "DungOffice"), (Join-Path $env:LOCALAPPDATA "DungOffice"), (Join-Path $env:APPDATA "Dung Office")) {
  if (Test-Path -LiteralPath $d) { Note "dữ liệu người dùng còn lại sau khi gỡ (bộ cài không xoá, có chủ ý?): $d ($(@(Get-ChildItem -LiteralPath $d -Recurse -File -ErrorAction SilentlyContinue).Count) tệp)" }
}
Get-ChildItem "HKCU:\Software" -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "Dung|Dũng" } | ForEach-Object { Note "khoá cài đặt của app còn lại: $($_.Name)" }

Say ""
Say "TỔNG KẾT: $script:fails lỗi"
exit ([int]($script:fails -gt 0))
