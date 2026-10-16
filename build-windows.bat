@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ==== WIFEBEATER Windows build ====

where git >nul 2>nul
if errorlevel 1 (
  echo.
  echo Git is required ^(CMake uses it to download JUCE^). Install it from https://git-scm.com/download/win and run this again.
  goto :fail
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo.
  echo Visual Studio was not found. Install Visual Studio 2022 Community with the "Desktop development with C++" workload.
  goto :fail
)
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
  echo.
  echo Visual Studio is installed but has no C++ tools. Open the Visual Studio Installer, click Modify,
  echo and tick "Desktop development with C++".
  goto :fail
)
echo Using Visual Studio at: %VSPATH%

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto :fail
rem CMake and Ninja that ship with Visual Studio
set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DWB_BUILD_TESTS=OFF
if errorlevel 1 goto :fail
cmake --build build --parallel
if errorlevel 1 goto :fail

set "OUT="
if exist "build\WIFEBEATER_artefacts\Release\VST3\WIFEBEATER.vst3" set "OUT=build\WIFEBEATER_artefacts\Release\VST3\WIFEBEATER.vst3"
if not defined OUT if exist "build\WIFEBEATER_artefacts\VST3\WIFEBEATER.vst3" set "OUT=build\WIFEBEATER_artefacts\VST3\WIFEBEATER.vst3"
if not defined OUT (
  echo Build finished but the .vst3 was not found under build\WIFEBEATER_artefacts.
  goto :fail
)

echo.
echo Built: %OUT%
set "DEST=%CommonProgramFiles%\VST3\WIFEBEATER.vst3"
xcopy "%OUT%" "%DEST%\" /E /I /Y /Q >nul 2>nul
if errorlevel 1 (
  echo Could not copy into "%CommonProgramFiles%\VST3" ^(needs Administrator^).
  echo Copy this folder there yourself:  %CD%\%OUT%
  echo   to:  %CommonProgramFiles%\VST3\
) else (
  echo Installed to: %DEST%
  echo Rescan plugins in your DAW.
)
echo A standalone app is in build\WIFEBEATER_artefacts\...\Standalone\ if you want to try it without a DAW.
echo.
pause
exit /b 0

:fail
echo.
echo Build failed. Scroll up for the error and send it to me.
pause
exit /b 1
