#define Ver "0.22.0"
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
#define NeonPlugin "INVERSO NEON.vst3"
#define SourceNeon "..\build\InversoNeon_artefacts\Release\VST3\" + NeonPlugin
#define HaloPlugin "INVERSO HALO.vst3"
#define SourceHalo "..\build\InversoHalo_artefacts\Release\VST3\" + HaloPlugin
#define MasterPlugin "INVERSO MASTER.vst3"
#define SourceMaster "..\build\InversoMaster_artefacts\Release\VST3\" + MasterPlugin
#define KickPlugin "INVERSO KICK.vst3"
#define SourceKick "..\build\InversoKick_artefacts\Release\VST3\" + KickPlugin
#define SnarePlugin "INVERSO SNARE.vst3"
#define SourceSnare "..\build\InversoSnare_artefacts\Release\VST3\" + SnarePlugin
#define TomsPlugin "INVERSO TOMS.vst3"
#define SourceToms "..\build\InversoToms_artefacts\Release\VST3\" + TomsPlugin
#define HatsPlugin "INVERSO HATS.vst3"
#define SourceHats "..\build\InversoHats_artefacts\Release\VST3\" + HatsPlugin
#define CymbalsPlugin "INVERSO CYMBALS.vst3"
#define SourceCymbals "..\build\InversoCymbals_artefacts\Release\VST3\" + CymbalsPlugin
#define OverheadsPlugin "INVERSO OVERHEADS.vst3"
#define SourceOverheads "..\build\InversoOverheads_artefacts\Release\VST3\" + OverheadsPlugin
#define CapturePlugin "INVERSO CAPTURE.vst3"
#define SourceCapture "..\build\InversoCapture_artefacts\Release\VST3\" + CapturePlugin
#define Cabs "..\..\IRs\INVERSO CABS"
[Setup]
AppId={{E14A7E1F-FB29-4908-9CAA-051298D6B3D9}
AppName=INVERSO ABISMO
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO ABISMO, SAMPLER, DRUMS, RELICARIO, ENKI, NEON, CLEAR, TIDE, HALO, MASTER, as pistas de bateria e CAPTURE (VST3)
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
Source: "{#SourceNeon}\*"; DestDir: "{commoncf64}\VST3\{#NeonPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceHalo}\*"; DestDir: "{commoncf64}\VST3\{#HaloPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceMaster}\*"; DestDir: "{commoncf64}\VST3\{#MasterPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceKick}\*"; DestDir: "{commoncf64}\VST3\{#KickPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceSnare}\*"; DestDir: "{commoncf64}\VST3\{#SnarePlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceToms}\*"; DestDir: "{commoncf64}\VST3\{#TomsPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceHats}\*"; DestDir: "{commoncf64}\VST3\{#HatsPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceCymbals}\*"; DestDir: "{commoncf64}\VST3\{#CymbalsPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceOverheads}\*"; DestDir: "{commoncf64}\VST3\{#OverheadsPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceCapture}\*"; DestDir: "{commoncf64}\VST3\{#CapturePlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
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
Type: filesandordirs; Name: "{commoncf64}\VST3\{#NeonPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#HaloPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#MasterPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#KickPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#SnarePlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#TomsPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#HatsPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#CymbalsPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#OverheadsPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#CapturePlugin}"
