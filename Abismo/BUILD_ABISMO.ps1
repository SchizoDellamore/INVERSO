# -HostSmoke also builds and runs the JUCE host tests (slow: each compiles JUCE again).
# -NoLto skips link-time optimisation (faster build, same sound).
[CmdletBinding()]
param([string]$JucePath="",[string]$Generator="",[switch]$SkipInstaller,[switch]$HostSmoke,[switch]$NoLto)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=$PSScriptRoot
function Assert-Status([string]$step){if($LASTEXITCODE -ne 0){throw "$step failed with exit code $LASTEXITCODE"}}
if(-not $JucePath){$JucePath=Join-Path (Split-Path $root -Parent) 'JUCE'}
if(-not (Test-Path (Join-Path $JucePath 'CMakeLists.txt'))){throw "JUCE not found: $JucePath"}
# No generator given: CMake picks the newest Visual Studio installed.
$generatorArgs=if($Generator){@('-G',$Generator)}else{@()}
& cmake -S $root -B (Join-Path $root 'build') @generatorArgs -A x64 "-DJUCE_ROOT=$JucePath" -DINVERSO_DSP_TESTS_ONLY=OFF "-DINVERSO_LTO=$(if($NoLto){'OFF'}else{'ON'})"
Assert-Status 'CMake configure'
$targets=@('InversoAbismo_VST3','InversoAbismoSampler_VST3','InversoAbismoDrums_VST3','InversoRelicario_VST3','InversoEnki_VST3',
 'abismo_engine','abismo_presets','abismo_stress','abismo_sampler','abismo_drums','relicario_engine','enki_engine')
if($HostSmoke){$targets+=@('InversoAbismoHostSmoke','InversoAbismoSamplerHostSmoke','InversoAbismoDrumsHostSmoke','InversoRelicarioHostSmoke','InversoEnkiHostSmoke')}
& cmake --build (Join-Path $root 'build') --config Release --target $targets --parallel 4
Assert-Status 'Build VST3'
if($HostSmoke){& ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure} else {& ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure -E host_smoke}
Assert-Status 'DSP and host tests'
$bundle=Join-Path $root 'build\InversoAbismo_artefacts\Release\VST3\INVERSO ABISMO.vst3'
if(-not (Test-Path -LiteralPath $bundle -PathType Container)){throw "Missing VST3 bundle $bundle"}
if(-not (Get-ChildItem -LiteralPath $bundle -Recurse -File | Where-Object {$_.Extension -eq '.vst3'})){throw 'Missing VST3 executable in bundle'}
$samplerBundle=Join-Path $root 'build\InversoAbismoSampler_artefacts\Release\VST3\INVERSO ABISMO SAMPLER.vst3'
if(-not (Get-ChildItem -LiteralPath $samplerBundle -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$_.Extension -eq '.vst3'})){throw 'Missing ABISMO SAMPLER VST3 executable'}
$drumsBundle=Join-Path $root 'build\InversoAbismoDrums_artefacts\Release\VST3\INVERSO ABISMO DRUMS.vst3'
if(-not (Get-ChildItem -LiteralPath $drumsBundle -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$_.Extension -eq '.vst3'})){throw 'Missing ABISMO DRUMS VST3 executable'}
$relicarioBundle=Join-Path $root 'build\InversoRelicario_artefacts\Release\VST3\INVERSO RELICARIO.vst3'
if(-not (Get-ChildItem -LiteralPath $relicarioBundle -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$_.Extension -eq '.vst3'})){throw 'Missing RELICARIO VST3 executable'}
$enkiBundle=Join-Path $root 'build\InversoEnki_artefacts\Release\VST3\INVERSO ENKI.vst3'
if(-not (Get-ChildItem -LiteralPath $enkiBundle -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$_.Extension -eq '.vst3'})){throw 'Missing ENKI VST3 executable'}
if(-not (Test-Path -LiteralPath (Join-Path $enkiBundle 'Contents\Resources\drums.bin'))){throw 'ENKI recordings missing from the bundle'}
if($SkipInstaller){exit 0}
$iscc=Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) 'Inno Setup 6\ISCC.exe'
if(-not (Test-Path $iscc)){$iscc=(Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source}
if(-not $iscc){throw 'Inno Setup 6 ISCC.exe not found'}
& $iscc (Join-Path $root 'Installer\INVERSO_ABISMO.iss')
Assert-Status 'Inno Setup'
if(-not (Test-Path (Join-Path $root 'dist\INVERSO_ABISMO_Setup_0.11.0_x64.exe'))){throw 'Installer missing'}
Write-Host 'SUCCESS: INVERSO ABISMO installer compiled.'
