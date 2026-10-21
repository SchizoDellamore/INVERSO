<##
Build Windows x64 VST3 and create an Inno Setup installer.
Prerequisites: Visual Studio 2022 C++ workload, CMake 3.22+, Inno Setup 6.3+.
If JUCE is not supplied, Git and network access are also required to clone JUCE 8.0.16-security.
Example: powershell -ExecutionPolicy Bypass -File .\BUILD_AND_PACKAGE.ps1 -JucePath C:\SDK\JUCE
##>
[CmdletBinding()]
param(
    [string]$JucePath = "",
    [string]$JuceTag = "8.0.16-security",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$SkipInstaller
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $PSScriptRoot

function Assert-ExitCode([string]$what) {
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)." }
}
function Find-ISCC() {
    $command = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $pf86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    $possible = @(
        (Join-Path $pf86 "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
        (Join-Path $pf86 "Inno Setup 7\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 7\ISCC.exe")
    )
    foreach ($p in $possible) { if (Test-Path -LiteralPath $p) { return $p } }
    return $null
}
if ($PSVersionTable.PSEdition -eq "Core" -and -not $IsWindows) {
    throw 'This build script must run on Windows x64.'
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake was not found. Install CMake 3.22+ and add it to PATH.'
}
if (-not $JucePath) { $JucePath = Join-Path $root 'JUCE' }
if (-not (Test-Path -LiteralPath (Join-Path $JucePath 'CMakeLists.txt'))) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw 'JUCE not found, and Git is unavailable. Supply -JucePath pointing to JUCE 8.'
    }
    Write-Host "Fetching JUCE tag $JuceTag (not bundled with INVERSO EQ)..."
    & git clone --depth 1 --branch $JuceTag 'https://github.com/juce-framework/JUCE.git' $JucePath
    Assert-ExitCode 'JUCE download'
}
$juceFile = Join-Path $JucePath 'CMakeLists.txt'
if (-not (Test-Path -LiteralPath $juceFile)) { throw "Invalid JUCE location: $JucePath" }
$build = Join-Path $root 'build'
# Configure the C++ plugin and portable DSP test targets.
& cmake -S $root -B $build -G $Generator -A x64 "-DJUCE_ROOT=$JucePath" "-DINVERSO_DSP_TESTS_ONLY=OFF"
Assert-ExitCode 'CMake configuration'
& cmake --build $build --config Release --target InversoEQ_VST3 inverso_eq_test inverso_eq_stress inverso_eq_automation inverso_eq_truepeak inverso_eq_instances --parallel 4
Assert-ExitCode 'Windows VST3 compilation'
& ctest --test-dir $build -C Release --output-on-failure
Assert-ExitCode 'DSP functional/stress tests'
$bundle = Join-Path $build 'InversoEQ_artefacts\Release\VST3\INVERSO EQ.vst3'
if (-not (Test-Path -LiteralPath $bundle -PathType Container)) {
    throw "The plugin bundle was not found at $bundle. Installer was NOT generated."
}
$binaries = @(Get-ChildItem -LiteralPath $bundle -Recurse -File | Where-Object { $_.Name -ieq 'INVERSO EQ.vst3' })
if ($binaries.Count -eq 0) { throw 'VST3 bundle does not contain its binary. Installer was NOT generated.' }
Write-Host "Plugin built: $bundle"
if ($SkipInstaller) {
    Write-Host 'Installer step omitted (-SkipInstaller).'
    exit 0
}
$iscc = Find-ISCC
if (-not $iscc) {
    throw "Plugin compiled, but Inno Setup ISCC.exe was not found. Install Inno Setup 6.3+ and rerun this script."
}
& $iscc (Join-Path $root 'Installer\INVERSO_EQ.iss')
Assert-ExitCode 'Inno Setup installer compilation'
$installer = Join-Path $root 'dist\INVERSO_EQ_Setup_0.5.0_x64.exe'
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw "Installer output missing: $installer" }
Write-Host "SUCCESS: $installer"
Write-Host 'Unsigned installer: Windows SmartScreen may warn. Close DAWs before installing.'
