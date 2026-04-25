; AppVersion can be overridden via /DAppVersion=x.y.z-beta.1 on the iscc command line.
; Falls back to reading the VERSION file.
#ifndef AppVersion
  #define VersionFile SourcePath + "\..\..\VERSION"
  #define FileHandle FileOpen(VersionFile)
  #if FileHandle
    #define AppVersion Trim(FileRead(FileHandle))
    #expr FileClose(FileHandle)
  #else
    #pragma error "Could not open VERSION file at: " + VersionFile
    #define AppVersion "2.0.0"
  #endif
#endif

[Setup]
AppName=October Production Co. Plugins
AppVersion={#AppVersion}
AppPublisher=October Production Co.
AppPublisherURL=https://github.com/gianteagle
DefaultDirName={autopf}\OctoberProductionCo
DefaultGroupName=October Production Co.
OutputDir=..\..\dist
OutputBaseFilename=OctoberPluginsSuite-{#AppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
LicenseFile=..\..\LICENSE
WizardStyle=modern
AppId={{F33D6872-B695-45F2-84C5-F2C1CA9D1855}

[Types]
Name: "full";   Description: "Install all plugins"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "octobass"; Description: "OctoBASS - Bass plugin";              Types: full
Name: "octobir";  Description: "OctobIR - Impulse response loader";   Types: full

[Files]
; OctoBASS VST3
Source: "..\..\build\release\plugins\octobass\juce\OctoBASS_artefacts\Release\VST3\OctoBASS.vst3\*"; \
  DestDir: "{commoncf}\VST3\OctoBASS.vst3"; \
  Flags: recursesubdirs ignoreversion; \
  Components: octobass

; OctobIR VST3
Source: "..\..\build\release\plugins\octobir\juce\OctobIR_artefacts\Release\VST3\OctobIR.vst3\*"; \
  DestDir: "{commoncf}\VST3\OctobIR.vst3"; \
  Flags: recursesubdirs ignoreversion; \
  Components: octobir

; Documentation
Source: "..\..\LICENSE";   DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall October Production Co. Plugins"; Filename: "{uninstallexe}"

[Code]
procedure UninstallLegacy(const KeyName: String);
var
  UninstallString: String;
  ResultCode: Integer;
begin
  if RegQueryStringValue(HKLM,
       'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\' + KeyName,
       'UninstallString', UninstallString) then
  begin
    UninstallString := RemoveQuotes(UninstallString);
    Exec(UninstallString, '/SILENT /SUPPRESSMSGBOX /NORESTART', '',
         SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

function InitializeSetup(): Boolean;
begin
  UninstallLegacy('{B3A7F2E1-9C4D-4E8B-A1F6-7D2E5C8B4A90}_is1');
  UninstallLegacy('OctobIR_is1');
  Result := True;
end;
