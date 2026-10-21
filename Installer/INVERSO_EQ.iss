; INVERSO EQ 0.4.0 — Inno Setup 6.3+ x64 VST3 installer
; Only compile AFTER the VST3 bundle is built by CMake.
#define AppVersion "0.4.0"
#define BundleName "INVERSO EQ.vst3"
#define BundleDir "..\build\InversoEQ_artefacts\Release\VST3\" + BundleName

[Setup]
AppId={{36C42E18-755A-4A2A-BA85-E35D556A1268}
AppName=INVERSO EQ
AppVersion={#AppVersion}
AppVerName=INVERSO EQ {#AppVersion}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO EQ (VST3)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_EQ_Setup_{#AppVersion}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no

[Files]
; Recursively install the full JUCE VST3 bundle (do not rename its internal binary).
Source: "{#BundleDir}\*"; DestDir: "{commoncf64}\VST3\{#BundleName}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
; Delete only the INVERSO EQ-specific bundle folder if any residue remains.
Type: filesandordirs; Name: "{commoncf64}\VST3\{#BundleName}"
