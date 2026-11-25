; Bộ cài OneClick ngoại tuyến cho Dũng Office (Inno Setup 6)
; Build: iscc /DSourceDir=<dist\DungOffice> /DAppVersion=0.1.0 installer\DungOffice.iss
; Cài cho người dùng hiện tại, KHÔNG cần quyền quản trị, không cần mạng.

#ifndef SourceDir
  #define SourceDir "..\build\dist\DungOffice"
#endif
#ifndef DictDir
  ; Thư mục từ điển tiếng Việt do scripts\package-windows.ps1 dựng NGOÀI thư mục ứng dụng (xem -DictViDir)
  #define DictDir "..\build\dist\dict-vi"
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6C1E7B53-2F7B-4C4B-9C59-0D0A1B2C3D4E}
AppName=Dũng Office
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\DungOffice
DefaultGroupName=Dũng Office
DisableProgramGroupPage=yes
DisableDirPage=yes
DisableReadyPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=DungOffice-Setup-{#AppVersion}-x64
SetupIconFile=..\app\resources\dungoffice.ico
UninstallDisplayIcon={app}\DungOffice.exe
; ultra64 + nén liền khối trên ~600 MB engine làm trình biên dịch 32-bit của Inno Setup hết bộ nhớ
Compression=lzma2/max
LZMADictionarySize=16384
SolidCompression=yes
LZMANumBlockThreads=1
WizardStyle=modern
LicenseFile=..\LICENSE
InfoBeforeFile=thong-tin.rtf
AppPublisher=Trần Văn Dũng
AppPublisherURL=https://github.com/dungmao2023-art/DUngoff
MinVersion=10.0
; Báo cho Explorer biết liên kết tệp đã đổi
ChangesAssociations=yes

[Types]
; Mặc định "Cài đầy đủ" = mọi thành phần được chọn sẵn; chọn "Tuỳ chọn" để bỏ từng thành phần.
Name: "full"; Description: "Cài đầy đủ (khuyến nghị, gồm từ điển chính tả tiếng Việt)"
Name: "compact"; Description: "Cài tối thiểu (không có từ điển)"
Name: "custom"; Description: "Tuỳ chọn"; Flags: iscustom

[Components]
Name: "main"; Description: "Dũng Office"; Types: full compact custom; Flags: fixed
; Dữ liệu GPL tách riêng: cài vào {app}\dictionaries\vi, không nằm trong tệp chạy hay engine; bỏ chọn được, gỡ được.
Name: "dict_vi"; Description: "Từ điển chính tả tiếng Việt (GPL v2)"; Types: full custom

[Languages]
; Vietnamese.isl là bản dịch không chính thức của Inno Setup (CI tải từ jrsoftware/issrc).
#ifexist "Vietnamese.isl"
Name: "vi"; MessagesFile: "Vietnamese.isl"
#else
Name: "en"; MessagesFile: "compiler:Default.isl"
#endif

[Tasks]
Name: "desktopicon"; Description: "Tạo biểu tượng trên màn hình nền"; GroupDescription: "Tuỳ chọn (đã chọn sẵn):"
Name: "startmenu"; Description: "Tạo lối tắt trong Start Menu"; GroupDescription: "Tuỳ chọn (đã chọn sẵn):"
; Liên kết tệp: mỗi loại một ô riêng. Ô nào CHƯA có phần mềm khác mở thì được chọn sẵn (xem [Code]); ô đã có phần mềm khác thì bỏ chọn sẵn.
; Windows 10/11 không cho bộ cài chiếm quyền mặc định thay người dùng: Dũng Office luôn hiện trong "Mở bằng"; muốn đặt làm mặc định thì chọn trong Cài đặt Windows.
Name: "assoc_docx"; Description: "Cho phép mở tệp Word (.docx) bằng Dũng Office"; GroupDescription: "Liên kết tệp (Windows vẫn do bạn chọn phần mềm mặc định):"; Flags: unchecked
Name: "assoc_xlsx"; Description: "Cho phép mở tệp Excel (.xlsx) bằng Dũng Office"; GroupDescription: "Liên kết tệp (Windows vẫn do bạn chọn phần mềm mặc định):"; Flags: unchecked
Name: "assoc_pptx"; Description: "Cho phép mở tệp PowerPoint (.pptx) bằng Dũng Office"; GroupDescription: "Liên kết tệp (Windows vẫn do bạn chọn phần mềm mặc định):"; Flags: unchecked
Name: "assoc_pdf"; Description: "Cho phép mở tệp PDF (.pdf) bằng Dũng Office"; GroupDescription: "Liên kết tệp (Windows vẫn do bạn chọn phần mềm mặc định):"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Components: main; Flags: ignoreversion recursesubdirs createallsubdirs
; Từ điển tiếng Việt (GPL): thành phần riêng, thư mục riêng. Nếu bản dựng không có từ điển thì thành phần này rỗng.
Source: "{#DictDir}\*"; DestDir: "{app}\dictionaries\vi"; Components: dict_vi; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\Dũng Office"; Filename: "{app}\DungOffice.exe"; Tasks: startmenu
Name: "{userdesktop}\Dũng Office"; Filename: "{app}\DungOffice.exe"; Tasks: desktopicon

[Registry]
; Thêm "Mở bằng Dũng Office" (không chiếm liên kết mặc định của người dùng)
Root: HKCU; Subkey: "Software\Classes\Applications"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\Applications\DungOffice.exe\shell\open\command"; ValueType: string; ValueData: """{app}\DungOffice.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\DungOffice.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "Dũng Office"; Flags: uninsdeletekey
; Khoá cha rỗng còn sót lại sau khi gỡ (kiểm bằng scripts/test-installer.ps1). Inno gỡ theo thứ tự ngược: các dòng này đứng
; TRƯỚC dòng con nên được xử lý SAU (khi con đã xoá), và chỉ xoá nếu khoá thật sự rỗng (không đụng dữ liệu của phần mềm khác).
Root: HKCU; Subkey: "Software\Classes\.xlsx"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.xlsx\OpenWithList"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.docx"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.docx\OpenWithList"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.pptx"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.pptx\OpenWithList"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.pdf"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.pdf\OpenWithList"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.xlsx\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.docx\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.pptx\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.pdf\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey

; --- Liên kết tệp theo lựa chọn ---
; ProgID riêng từng loại + khai báo trong OpenWithProgids (hiện trong "Mở bằng"). KHÔNG ghi UserChoice (Windows bảo vệ bằng mã băm).
; Giá trị mặc định của đuôi chỉ được tạo khi đuôi chưa có giá trị (createvalueifdoesntexist) -> không đè phần mềm khác;
; khi gỡ cài đặt, giá trị đó chỉ bị xoá nếu vẫn trỏ về Dũng Office (xem [Code]).
Root: HKCU; Subkey: "Software\Classes\DungOffice.docx"; ValueType: string; ValueData: "Tài liệu Word (Dũng Office)"; Flags: uninsdeletekey; Tasks: assoc_docx
Root: HKCU; Subkey: "Software\Classes\DungOffice.docx\DefaultIcon"; ValueType: string; ValueData: "{app}\DungOffice.exe,0"; Tasks: assoc_docx
Root: HKCU; Subkey: "Software\Classes\DungOffice.docx\shell\open\command"; ValueType: string; ValueData: """{app}\DungOffice.exe"" ""%1"""; Tasks: assoc_docx
Root: HKCU; Subkey: "Software\Classes\.docx\OpenWithProgids"; ValueType: string; ValueName: "DungOffice.docx"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: assoc_docx
Root: HKCU; Subkey: "Software\Classes\.docx"; ValueType: string; ValueData: "DungOffice.docx"; Flags: createvalueifdoesntexist uninsdeletekeyifempty; Tasks: assoc_docx
Root: HKCU; Subkey: "Software\Classes\DungOffice.xlsx"; ValueType: string; ValueData: "Bảng tính Excel (Dũng Office)"; Flags: uninsdeletekey; Tasks: assoc_xlsx
Root: HKCU; Subkey: "Software\Classes\DungOffice.xlsx\DefaultIcon"; ValueType: string; ValueData: "{app}\DungOffice.exe,0"; Tasks: assoc_xlsx
Root: HKCU; Subkey: "Software\Classes\DungOffice.xlsx\shell\open\command"; ValueType: string; ValueData: """{app}\DungOffice.exe"" ""%1"""; Tasks: assoc_xlsx
Root: HKCU; Subkey: "Software\Classes\.xlsx\OpenWithProgids"; ValueType: string; ValueName: "DungOffice.xlsx"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: assoc_xlsx
Root: HKCU; Subkey: "Software\Classes\.xlsx"; ValueType: string; ValueData: "DungOffice.xlsx"; Flags: createvalueifdoesntexist uninsdeletekeyifempty; Tasks: assoc_xlsx
Root: HKCU; Subkey: "Software\Classes\DungOffice.pptx"; ValueType: string; ValueData: "Trình chiếu PowerPoint (Dũng Office)"; Flags: uninsdeletekey; Tasks: assoc_pptx
Root: HKCU; Subkey: "Software\Classes\DungOffice.pptx\DefaultIcon"; ValueType: string; ValueData: "{app}\DungOffice.exe,0"; Tasks: assoc_pptx
Root: HKCU; Subkey: "Software\Classes\DungOffice.pptx\shell\open\command"; ValueType: string; ValueData: """{app}\DungOffice.exe"" ""%1"""; Tasks: assoc_pptx
Root: HKCU; Subkey: "Software\Classes\.pptx\OpenWithProgids"; ValueType: string; ValueName: "DungOffice.pptx"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: assoc_pptx
Root: HKCU; Subkey: "Software\Classes\.pptx"; ValueType: string; ValueData: "DungOffice.pptx"; Flags: createvalueifdoesntexist uninsdeletekeyifempty; Tasks: assoc_pptx
Root: HKCU; Subkey: "Software\Classes\DungOffice.pdf"; ValueType: string; ValueData: "Tài liệu PDF (Dũng Office)"; Flags: uninsdeletekey; Tasks: assoc_pdf
Root: HKCU; Subkey: "Software\Classes\DungOffice.pdf\DefaultIcon"; ValueType: string; ValueData: "{app}\DungOffice.exe,0"; Tasks: assoc_pdf
Root: HKCU; Subkey: "Software\Classes\DungOffice.pdf\shell\open\command"; ValueType: string; ValueData: """{app}\DungOffice.exe"" ""%1"""; Tasks: assoc_pdf
Root: HKCU; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "DungOffice.pdf"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: assoc_pdf
Root: HKCU; Subkey: "Software\Classes\.pdf"; ValueType: string; ValueData: "DungOffice.pdf"; Flags: createvalueifdoesntexist uninsdeletekeyifempty; Tasks: assoc_pdf

[Run]
Filename: "{app}\DungOffice.exe"; Description: "Mở Dũng Office"; Flags: nowait postinstall skipifsilent

[Code]
// Đuôi đã có phần mềm khác mở? (lựa chọn của người dùng trong Windows, hoặc giá trị mặc định ở HKCR)
function ExtHasOtherHandler(const Ext: String): Boolean;
var
  S: String;
begin
  Result := False;
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\' + Ext + '\UserChoice', 'ProgId', S) then
    if S <> '' then
    begin
      Result := True;
      Exit;
    end;
  if RegQueryStringValue(HKCR, Ext, '', S) then
    if (S <> '') and (Pos('DungOffice.', S) <> 1) then
      Result := True;
end;

var
  AssocPreselected: Boolean;

// Chọn sẵn ô liên kết cho loại tệp CHƯA có phần mềm nào mở; loại đã có thì bỏ chọn để người dùng tự quyết. Chỉ làm một lần.
procedure PreselectAssociations;
var
  Exts: array[0..3] of String;
  I: Integer;
begin
  Exts[0] := 'docx';
  Exts[1] := 'xlsx';
  Exts[2] := 'pptx';
  Exts[3] := 'pdf';
  for I := 0 to 3 do
  begin
    if ExtHasOtherHandler('.' + Exts[I]) then
      WizardSelectTasks('!assoc_' + Exts[I])
    else
      WizardSelectTasks('assoc_' + Exts[I]);
  end;
end;

var
  DictPage: TOutputMsgMemoWizardPage;

procedure InitializeWizard;
begin
  DictPage := CreateOutputMsgMemoPage(wpSelectComponents,
    'Từ điển chính tả tiếng Việt',
    'Giấy phép và nguồn của thành phần tuỳ chọn này',
    'Vui lòng đọc thông tin dưới đây. Nếu không muốn cài từ điển, bấm "Quay lại" và bỏ chọn thành phần này.',
    'Đây là DỮ LIỆU RIÊNG (tệp .dic và .aff), không phải mã chương trình của Dũng Office.' + #13#10 +
    'Nó được cài vào thư mục riêng "dictionaries\vi" cạnh ứng dụng; có thể xoá bất cứ lúc nào, ứng dụng vẫn chạy bình thường.' + #13#10 + #13#10 +
    'Tác giả và nguồn:' + #13#10 +
    '- Danh sách từ gốc: Hồ Ngọc Đức, Dự án Từ điển tiếng Việt miễn phí (gói GNU Aspell 0.60 Vietnamese, bản 0.01.1-1, 2004).' + #13#10 +
    '- Chuyển sang Hunspell: László Németh. Sửa luật và đóng gói: Ivan Garcia, Nguyễn Xuân Minh (hunspell-vi).' + #13#10 +
    '- Kho gốc: https://github.com/1ec5/hunspell-vi (commit 39cc647f0cf3bdfc579980a2c3aac3f6f4203fe6).' + #13#10 + #13#10 +
    'Giấy phép: GNU General Public License. Theo tệp README gốc, danh sách từ có điều khoản GPL phiên bản 2; phần đóng gói của hunspell-vi theo GPL phiên bản 3 (hoặc mới hơn).' + #13#10 +
    'Bạn có quyền dùng, sao chép, sửa và phân phối lại theo các giấy phép đó; không có bảo hành. Mã nguồn chính là các tệp .dic/.aff được cài kèm, bản gốc ở liên kết trên.' + #13#10 +
    'Văn bản đầy đủ: tệp GPL-2.0.txt và GPL-3.0.txt trong thư mục dictionaries\vi sau khi cài.' + #13#10 + #13#10 +
    'Từ điển này không được liên kết vào chương trình Dũng Office (giấy phép MPL-2.0); hai bên chỉ được phân phối chung trong một bộ cài.');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = DictPage.ID then
    Result := not WizardIsComponentSelected('dict_vi');
end;

procedure CurrentPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpSelectTasks) and (not AssocPreselected) then
  begin
    AssocPreselected := True;
    PreselectAssociations;
  end;
end;

// Gỡ cài đặt: chỉ xoá giá trị mặc định của đuôi nếu nó vẫn trỏ về Dũng Office (không xoá lựa chọn của phần mềm khác).
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Exts: array[0..3] of String;
  I: Integer;
  S: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exts[0] := 'docx';
    Exts[1] := 'xlsx';
    Exts[2] := 'pptx';
    Exts[3] := 'pdf';
    for I := 0 to 3 do
      if RegQueryStringValue(HKCU, 'Software\Classes\.' + Exts[I], '', S) then
        if S = 'DungOffice.' + Exts[I] then
          RegDeleteValue(HKCU, 'Software\Classes\.' + Exts[I], '');
  end;
end;
