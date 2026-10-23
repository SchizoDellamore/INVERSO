#define Ver "0.1.0"
#define Plugin "INVERSO OddSynth.vst3"
[Setup]
AppId={{DEA612A2-6F70-48BB-9E90-077A1135C56D}
AppName=INVERSO OddSynth
AppVersion={#Ver}
AppPublisher=INVERSO STUDIOS
DefaultDirName={autopf}\INVERSO\OddSynth
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO OddSynth (VST3 e aplicativo)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=dist
OutputBaseFilename=INVERSO_OddSynth_Setup_{#Ver}_x64
CloseApplications=yes
RestartApplications=no
[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
[Files]
Source: "payload\{#Plugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "payload\INVERSO OddSynth.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "payload\*.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{autoprograms}\INVERSO\OddSynth"; Filename: "{app}\INVERSO OddSynth.exe"
[Messages]
FinishedLabel=O INVERSO OddSynth foi instalado.%n%nAbra o Cubase, adicione uma pista de instrumento e procure INVERSO OddSynth. O aplicativo independente esta no menu Iniciar, em INVERSO.
