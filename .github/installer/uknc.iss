; Inno Setup script for the Windows package -- see
; .github/workflows/build.yml, which passes everything in:
;
;   ISCC /DAppVersion=... /DSourceDir=... /DOutputDir=... /DOutputName=...
;
; The installed tree is the toolchain root as it comes out of the build,
; unchanged: gcc\bin, gcc\xgcc, debugger\bin, rom, resources, examples.
; Everything in it finds its own way around from where it sits, so there
; is nothing to configure afterwards -- except PATH, which is offered
; below because typing pdp11-uknc-rt11-gcc is the point of having it.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
#ifndef OutputName
  #define OutputName "uknc-toolchain"
#endif

[Setup]
AppName=UKNC toolchain
AppVersion={#AppVersion}
AppPublisher=wdigger
AppPublisherURL=https://github.com/wdigger/uknc
DefaultDirName={autopf}\UKNC
DefaultGroupName=UKNC
; Per-user by default, so that installing needs no administrator: this
; is a compiler and an emulator, not a system component.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesEnvironment=yes
WizardStyle=modern
LicenseFile=
DisableProgramGroupPage=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "addtopath"; \
  Description: "Добавить gcc\bin в PATH (тогда pdp11-uknc-rt11-gcc доступен из любой папки)"; \
  GroupDescription: "Настройка:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; \
  Flags: recursesubdirs createallsubdirs ignoreversion

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; \
  ValueData: "{olddata};{app}\gcc\bin"; \
  Tasks: addtopath; Check: NotAlreadyOnPath(ExpandConstant('{app}\gcc\bin'))

[Icons]
Name: "{group}\УКНЦ: эмулятор"; Filename: "{app}\debugger\bin\ukncbtldebug.exe"; \
  Parameters: "--help"
Name: "{group}\Удалить УКНЦ"; Filename: "{uninstallexe}"

[Code]
// PATH is a semicolon-separated list that the installer would otherwise
// happily grow a second copy of every time it runs.
function NotAlreadyOnPath(Dir: string): Boolean;
var
  Path: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', Path) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Lowercase(Dir) + ';', ';' + Lowercase(Path) + ';') = 0;
end;
