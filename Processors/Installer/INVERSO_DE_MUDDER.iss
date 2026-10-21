; INVERSO DE-MUDDER standalone installer 0.1.1
; Keep independent AppId from the combined Processors installer.
#define Ver "0.1.1"
#define Plugin "INVERSO DE-MUDDER.vst3"
#define SourcePlugin "..\build\InversoDeMudder_artefacts\Release\VST3\" + Plugin
[Setup]
AppId={{D25939FC-7D39-4F1F-B21A-D40D1A2E129B}
AppName=INVERSO DE-MUDDER
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO DE-MUDDER (VST3)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_DE_MUDDER_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#SourcePlugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Plugin}"
