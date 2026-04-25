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
AppName=OctobIR
AppVersion={#AppVersion}
AppPublisher=OctobIR
AppPublisherURL=https://github.com/gianteagle/OctobIR
DefaultDirName={autopf}\OctobIR
DefaultGroupName=OctobIR
OutputDir=../../../dist
OutputBaseFilename=OctobIR-{#AppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
LicenseFile=../../../LICENSE
WizardStyle=modern

[Files]
; VST3 Plugin
Source: "..\..\..\build\release\plugins\octobir\juce\OctobIR_artefacts\Release\VST3\OctobIR.vst3\*"; \
  DestDir: "{commoncf}\VST3\OctobIR.vst3"; \
  Flags: recursesubdirs

; Documentation
Source: "..\..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall OctobIR"; Filename: "{uninstallexe}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
