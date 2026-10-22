[CmdletBinding()]
param([string]$JucePath="",[string]$Generator="Visual Studio 17 2022",[switch]$SkipInstaller)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=$PSScriptRoot
function Assert-Step([string]$name){if($LASTEXITCODE -ne 0){throw "$name failed with $LASTEXITCODE"}}
if(-not $JucePath){$JucePath=Join-Path (Split-Path $root -Parent) 'JUCE'}
if(-not (Test-Path (Join-Path $JucePath 'CMakeLists.txt'))){throw "JUCE not found at $JucePath"}
& cmake -S $root -B (Join-Path $root 'build') -G $Generator -A x64 "-DJUCE_ROOT=$JucePath"
Assert-Step 'CMake configure'
& cmake --build (Join-Path $root 'build') --config Release --target InversoDrum_VST3 inverso_drum_engine inverso_drum_extended inverso_drum_fx_audible inverso_drum_velocity inverso_drum_event_fx InversoDrumHostSmoke --parallel 4
Assert-Step 'Plugin build'
& ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
Assert-Step 'Tests'
$plugin=Join-Path $root 'build\InversoDrum_artefacts\Release\VST3\INVERSO DRUM.vst3'
if(-not (Test-Path -LiteralPath $plugin -PathType Container)){throw 'Plugin bundle missing'}
if(-not (Get-ChildItem -LiteralPath $plugin -Recurse -File | Where-Object {$_.Extension -eq '.vst3'})){throw 'Plugin binary missing'}
if($SkipInstaller){exit 0}
$iscc=Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) 'Inno Setup 6\ISCC.exe'
if(-not (Test-Path $iscc)){$iscc=(Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source}
if(-not $iscc){throw 'Inno Setup 6 not found'}
& $iscc (Join-Path $root 'Installer\INVERSO_DRUM.iss')
Assert-Step 'Installer'
if(-not (Test-Path (Join-Path $root 'dist\INVERSO_DRUM_Setup_0.5.0_x64.exe'))){throw 'Installer missing'}
Write-Host 'INVERSO DRUM 0.5.0 built and tested.'
