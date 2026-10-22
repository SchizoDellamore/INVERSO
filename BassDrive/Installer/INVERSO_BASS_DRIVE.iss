#define Ver "0.3.0"
#define Plugin "INVERSO BASS DRIVE.vst3"
#define SourcePlugin "..\build\InversoBassDrive_artefacts\Release\VST3\" + Plugin
[Setup]
AppId={{B359ACE9-AB7B-4925-8924-1B8C3195D74B}
AppName=INVERSO BASS DRIVE
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO BASS DRIVE (VST3)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_BASS_DRIVE_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#SourcePlugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Plugin}"
