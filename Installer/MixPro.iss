; MIX PRO — BY ORYMY  (Inno Setup 6)
#define AppName "MIX PRO — BY ORYMY"
#define AppVersion "1.0.0"
[Setup]
AppId={{7A1C2E55-3B0F-4C6D-9D41-0F5E7C2A9B10}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=ORYMY · YMY Records
DefaultDirName={autopf}\ORYMY\MIX PRO
DefaultGroupName=ORYMY\MIX PRO
OutputDir=Output
OutputBaseFilename=MIX_PRO_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName}
DisableProgramGroupPage=yes
[Files]
; VST3 -> C:\Program Files\Common Files\VST3\
Source: "..\dist\vst3\MIX PRO.vst3\*"; DestDir: "{commoncf64}\VST3\MIX PRO.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\dist\standalone\MIX PRO.exe"; DestDir: "{app}"; Flags: ignoreversion
; Factory presets: shared folder, never overwrites user files
Source: "..\Presets\*"; DestDir: "{commonappdata}\ORYMY\MIX PRO\Presets"; Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\MIX PRO"; Filename: "{app}\MIX PRO.exe"
Name: "{group}\Uninstall MIX PRO"; Filename: "{uninstallexe}"
Name: "{autodesktop}\MIX PRO"; Filename: "{app}\MIX PRO.exe"; Tasks: desktopicon
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  if not DirExists(ExpandConstant('{commoncf64}\VST3')) then
    ForceDirectories(ExpandConstant('{commoncf64}\VST3'));
end;
