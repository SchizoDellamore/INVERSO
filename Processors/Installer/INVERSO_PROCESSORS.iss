; INVERSO PROCESSORS 0.1.0 - bundles two independent VST3 processors.
#define Ver "0.1.0"
#define Mud "INVERSO DE-MUDDER.vst3"
#define Ann "INVERSO DE-ANNOYANCE.vst3"
#define MudSource "..\build\InversoDeMudder_artefacts\Release\VST3\" + Mud
#define AnnSource "..\build\InversoDeAnnoyance_artefacts\Release\VST3\" + Ann
[Setup]
AppId={{394715E6-0E68-4197-B9A5-79B6415B2B1A}
AppName=INVERSO PROCESSORS
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO PROCESSORS (VST3)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_PROCESSORS_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#MudSource}\*"; DestDir: "{commoncf64}\VST3\{#Mud}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AnnSource}\*"; DestDir: "{commoncf64}\VST3\{#Ann}"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Mud}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Ann}"
