#define Ver "0.1.0"
#define Plugin "INVERSO WaveMover.vst3"

[Setup]
AppId={{3BCFDD85-9AE0-4660-BD27-F9E516066317}
AppName=INVERSO WaveMover
AppVersion={#Ver}
AppPublisher=INVERSO
DefaultDirName={autopf}\INVERSO\WaveMover
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=INVERSO WaveMover (VST3 e aplicativo)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=dist
OutputBaseFilename=INVERSO_WaveMover_Setup_{#Ver}_x64
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
InfoBeforeFile=INSTALAR.txt

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Files]
Source: "payload\{#Plugin}\*"; DestDir: "{commoncf64}\VST3\{#Plugin}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "payload\INVERSO WaveMover.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "payload\LEIA-ME.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "payload\ThirdParty\*"; DestDir: "{app}\ThirdParty"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "INSTALAR.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\INVERSO\WaveMover"; Filename: "{app}\INVERSO WaveMover.exe"

[Messages]
FinishedLabel=O INVERSO WaveMover foi instalado.%n%nAbra o Cubase e procure INVERSO WaveMover nos efeitos de inserção VST3. O aplicativo independente também está disponível no menu Iniciar, em INVERSO.
