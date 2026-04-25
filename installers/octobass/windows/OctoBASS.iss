; AppVersion can be overridden via /DAppVersion=x.y.z-beta.1 on the iscc command line.
; Falls back to reading the VERSION file.
#ifndef AppVersion
  #define VersionFile SourcePath + "\..\..\..\VERSION"
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
AppName=OctoBASS
AppVersion={#AppVersion}
AppPublisher=October Production Co.
AppPublisherURL=https://github.com/gianteagle/OctoBASS
DefaultDirName={autopf}\OctoBASS
DefaultGroupName=OctoBASS
OutputDir=../../../dist
OutputBaseFilename=OctoBASS-{#AppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
LicenseFile=../../../LICENSE
WizardStyle=modern
AppId={{B3A7F2E1-9C4D-4E8B-A1F6-7D2E5C8B4A90}

[Files]
; VST3 Plugin
Source: "..\..\..\build\release\plugins\octobass\juce\OctoBASS_artefacts\Release\VST3\OctoBASS.vst3\*"; \
  DestDir: "{commoncf}\VST3\OctoBASS.vst3"; \
  Flags: recursesubdirs

; Documentation
Source: "..\..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall OctoBASS"; Filename: "{uninstallexe}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
