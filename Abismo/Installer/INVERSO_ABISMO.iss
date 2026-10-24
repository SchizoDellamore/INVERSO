#define Ver "0.16.0"
#define Plugin "INVERSO ABISMO.vst3"
#define SourcePlugin "..\build\InversoAbismo_artefacts\Release\VST3\" + Plugin
#define SamplerPlugin "INVERSO ABISMO SAMPLER.vst3"
#define SourceSampler "..\build\InversoAbismoSampler_artefacts\Release\VST3\" + SamplerPlugin
#define DrumsPlugin "INVERSO ABISMO DRUMS.vst3"
#define SourceDrums "..\build\InversoAbismoDrums_artefacts\Release\VST3\" + DrumsPlugin
#define RelicarioPlugin "INVERSO RELICARIO.vst3"
#define SourceRelicario "..\build\InversoRelicario_artefacts\Release\VST3\" + RelicarioPlugin
#define EnkiPlugin "INVERSO ENKI.vst3"
#define SourceEnki "..\build\InversoEnki_artefacts\Release\VST3\" + EnkiPlugin
#define FurnacePlugin "INVERSO FURNACE.vst3"
#define ClearPlugin "INVERSO CLEAR.vst3"
#define SourceClear "..\build\InversoClear_artefacts\Release\VST3\" + ClearPlugin
#define TidePlugin "INVERSO TIDE.vst3"
#define SourceTide "..\build\InversoTide_artefacts\Release\VST3\" + TidePlugin
#define Cabs "..\..\IRs\INVERSO CABS"
[Setup]
AppId={{E14A7E1F-FB29-4908-9CAA-051298D6B3D9}
AppName=INVERSO ABISMO
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO ABISMO, SAMPLER, DRUMS, RELICARIO, ENKI, CLEAR e TIDE (VST3)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=INVERSO_ABISMO_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
UsedUserAreasWarning=no
[Files]
Source: "{#SourcePlugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceSampler}\*"; DestDir: "{commoncf64}\VST3\{#SamplerPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDrums}\*"; DestDir: "{commoncf64}\VST3\{#DrumsPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRelicario}\*"; DestDir: "{commoncf64}\VST3\{#RelicarioPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceEnki}\*"; DestDir: "{commoncf64}\VST3\{#EnkiPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceClear}\*"; DestDir: "{commoncf64}\VST3\{#ClearPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceTide}\*"; DestDir: "{commoncf64}\VST3\{#TidePlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
; INVERSO CABS: the impulse responses, in their own folder (Guitar and Bass), in Documents
Source: "{#Cabs}\*"; DestDir: "{userdocs}\INVERSO\INVERSO CABS"; Flags: ignoreversion recursesubdirs createallsubdirs
[InstallDelete]
; FURNACE was withdrawn: an update removes the old copy
Type: filesandordirs; Name: "{commoncf64}\VST3\{#FurnacePlugin}"
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Plugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#SamplerPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#DrumsPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#RelicarioPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#EnkiPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#FurnacePlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#ClearPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#TidePlugin}"
