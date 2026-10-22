[CmdletBinding()]
param([string]$JucePath="",[string]$Generator="Visual Studio 17 2022",[switch]$SkipInstaller)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=$PSScriptRoot
function Assert-Status([string]$step){if($LASTEXITCODE -ne 0){throw "$step failed with exit code $LASTEXITCODE"}}
if(-not $JucePath){$JucePath=Join-Path (Split-Path $root -Parent) 'JUCE'}
if(-not (Test-Path (Join-Path $JucePath 'CMakeLists.txt'))){throw "JUCE not found: $JucePath"}
& cmake -S $root -B (Join-Path $root 'build') -G $Generator -A x64 "-DJUCE_ROOT=$JucePath" -DINVERSO_DSP_TESTS_ONLY=OFF
Assert-Status 'CMake configure'
& cmake --build (Join-Path $root 'build') --config Release --target InversoBassDrive_VST3 bassdrive_functional bassdrive_stress bassdrive_passthrough bassdrive_color bassdrive_voicing InversoBassDriveHostSmoke --parallel 4
Assert-Status 'Build VST3'
& ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
Assert-Status 'DSP tests'
$bundle=Join-Path $root 'build\InversoBassDrive_artefacts\Release\VST3\INVERSO BASS DRIVE.vst3'
if(-not (Test-Path -LiteralPath $bundle -PathType Container)){throw "Missing VST3 bundle $bundle"}
if(-not (Get-ChildItem -LiteralPath $bundle -Recurse -File | Where-Object {$_.Extension -eq '.vst3'})){throw 'Missing VST3 executable in bundle'}
if($SkipInstaller){exit 0}
$iscc=Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) 'Inno Setup 6\ISCC.exe'
if(-not (Test-Path $iscc)){$iscc=(Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source}
if(-not $iscc){throw 'Inno Setup 6 ISCC.exe not found'}
& $iscc (Join-Path $root 'Installer\INVERSO_BASS_DRIVE.iss')
Assert-Status 'Inno Setup'
if(-not (Test-Path (Join-Path $root 'dist\INVERSO_BASS_DRIVE_Setup_0.4.0_x64.exe'))){throw 'Installer missing'}
Write-Host 'SUCCESS: INVERSO BASS DRIVE installer compiled.'
