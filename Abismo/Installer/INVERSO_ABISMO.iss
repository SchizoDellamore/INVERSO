#define Ver "0.10.0"
#define Plugin "INVERSO ABISMO.vst3"
#define SourcePlugin "..\build\InversoAbismo_artefacts\Release\VST3\" + Plugin
#define SamplerPlugin "INVERSO ABISMO SAMPLER.vst3"
#define SourceSampler "..\build\InversoAbismoSampler_artefacts\Release\VST3\" + SamplerPlugin
#define DrumsPlugin "INVERSO ABISMO DRUMS.vst3"
#define SourceDrums "..\build\InversoAbismoDrums_artefacts\Release\VST3\" + DrumsPlugin
#define RelicarioPlugin "INVERSO RELICARIO.vst3"
#define SourceRelicario "..\build\InversoRelicario_artefacts\Release\VST3\" + RelicarioPlugin
[Setup]
AppId={{E14A7E1F-FB29-4908-9CAA-051298D6B3D9}
AppName=INVERSO ABISMO
AppVersion={#Ver}
AppPublisher=Inverso Studios
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO ABISMO, SAMPLER, DRUMS e RELICARIO (VST3 instruments)
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
[Files]
Source: "{#SourcePlugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceSampler}\*"; DestDir: "{commoncf64}\VST3\{#SamplerPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDrums}\*"; DestDir: "{commoncf64}\VST3\{#DrumsPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRelicario}\*"; DestDir: "{commoncf64}\VST3\{#RelicarioPlugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Plugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#SamplerPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#DrumsPlugin}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#RelicarioPlugin}"
