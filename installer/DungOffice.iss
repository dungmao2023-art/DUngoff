; Bộ cài OneClick ngoại tuyến cho Dũng Office (Inno Setup 6)
; Build: iscc /DSourceDir=<dist\DungOffice> /DAppVersion=0.1.0 installer\DungOffice.iss
; Cài cho người dùng hiện tại, KHÔNG cần quyền quản trị, không cần mạng.

#ifndef SourceDir
  #define SourceDir "..\build\dist\DungOffice"
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6C1E7B53-2F7B-4C4B-9C59-0D0A1B2C3D4E}
AppName=Dũng Office
AppVersion={#AppVersion}
AppPublisher=Dũng Office
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
Compression=lzma2/ultra64
SolidCompression=yes
LZMANumBlockThreads=4
WizardStyle=modern
LicenseFile=..\LICENSE
MinVersion=10.0

[Languages]
; Vietnamese.isl là bản dịch không chính thức của Inno Setup (CI tải từ jrsoftware/issrc).
#ifexist "Vietnamese.isl"
Name: "vi"; MessagesFile: "Vietnamese.isl"
#else
Name: "en"; MessagesFile: "compiler:Default.isl"
#endif

[Tasks]
Name: "desktopicon"; Description: "Tạo biểu tượng trên màn hình nền"; GroupDescription: "Tuỳ chọn:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Dũng Office"; Filename: "{app}\DungOffice.exe"
Name: "{userdesktop}\Dũng Office"; Filename: "{app}\DungOffice.exe"; Tasks: desktopicon

[Registry]
; Thêm "Mở bằng Dũng Office" (không chiếm liên kết mặc định của người dùng)
Root: HKCU; Subkey: "Software\Classes\Applications\DungOffice.exe\shell\open\command"; ValueType: string; ValueData: """{app}\DungOffice.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\DungOffice.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "Dũng Office"
Root: HKCU; Subkey: "Software\Classes\.xlsx\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.docx\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.pptx\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.pdf\OpenWithList\DungOffice.exe"; Flags: uninsdeletekey

[Run]
Filename: "{app}\DungOffice.exe"; Description: "Mở Dũng Office"; Flags: nowait postinstall skipifsilent
