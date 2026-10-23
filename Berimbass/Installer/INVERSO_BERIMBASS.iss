#define Ver "0.2.0"
#define Plugin "INVERSO BERIMBASS.vst3"
#define SourcePlugin "..\build\InversoBerimbass_artefacts\Release\VST3\" + Plugin
[Setup]
AppId={{10C29B8A-232C-425F-99A5-A830B32A390D}
AppName=INVERSO BERIMBASS
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO BERIMBASS (VST3 instrument)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_BERIMBASS_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#SourcePlugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Plugin}"
