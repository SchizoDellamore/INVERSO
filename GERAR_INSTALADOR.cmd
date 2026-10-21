@echo off
setlocal
cd /d "%~dp0"
echo ========================================================
echo  INVERSO EQ - COMPILAR VST3 E GERAR INSTALADOR WINDOWS
 echo ========================================================
echo Necessario: Visual Studio C++, CMake, Git, Inno Setup.
echo O projeto nao inclui um VST3 precompilado.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BUILD_AND_PACKAGE.ps1"
if errorlevel 1 (
 echo.
 echo Falha na compilacao/empacotamento. Leia o erro acima.
 pause
 exit /b 1
)
echo.
echo Instalador salvo na pasta dist.
pause
