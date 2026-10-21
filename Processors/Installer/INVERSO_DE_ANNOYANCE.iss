; INVERSO DE-ANNOYANCE standalone installer 0.1.1
#define Ver "0.1.1"
#define Plugin "INVERSO DE-ANNOYANCE.vst3"
#define SourcePlugin "..\build\InversoDeAnnoyance_artefacts\Release\VST3\" + Plugin
[Setup]
AppId={{A3048527-623D-4623-BD80-9893608CB79F}
AppName=INVERSO DE-ANNOYANCE
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO DE-ANNOYANCE (VST3)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_DE_ANNOYANCE_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#SourcePlugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Plugin}"
