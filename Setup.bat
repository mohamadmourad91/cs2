@echo off
setlocal EnableExtensions
title Umayyad Strike - Setup
cd /d "%~dp0"
rem ============================================================
rem  Umayyad Strike - build + map generation (Windows)
rem  Double-click, or:  Setup.bat "D:\Program Files\Epic Games\UE_5.8"
rem  Everything is also written to Saved\setup_log.txt
rem ============================================================
if not exist "Saved" mkdir "Saved"
set "LOG=%CD%\Saved\setup_log.txt"
echo Umayyad Strike setup %DATE% %TIME% > "%LOG%"

set "PROJ=%CD%\UmayyadStrike.uproject"
if not exist "%PROJ%" goto :no_project

set "UE=%~1"
if not "%UE%"=="" goto :check_engine
set "UE=D:\Program Files\Epic Games\UE_5.8"
if exist "%UE%\Engine\Build\BatchFiles\Build.bat" goto :check_engine
set "UE=C:\Program Files\Epic Games\UE_5.8"
if exist "%UE%\Engine\Build\BatchFiles\Build.bat" goto :check_engine
set "UE=D:\unreal"
if exist "%UE%\Engine\Build\BatchFiles\Build.bat" goto :check_engine
goto :no_engine

:check_engine
if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" goto :no_engine
echo Project : %PROJ%
echo Engine  : %UE%
echo Engine: %UE% >> "%LOG%"

rem ---- Visual Studio (needed to compile C++) ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vs
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" goto :no_vs
echo VS      : %VSPATH%
echo VS: %VSPATH% >> "%LOG%"

echo.
echo [1/2] Compiling C++ (first time takes 5-15 minutes, please wait)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" UmayyadStrikeEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload > "Saved\build_log.txt" 2>&1
if errorlevel 1 goto :build_failed
echo Build OK.
echo Build OK >> "%LOG%"

echo.
echo [2/2] Opening Unreal Editor and building Umayyad Square...
echo       (the map is generated automatically after the editor loads)
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -ExecutePythonScript="%CD%\Content\Python\build_umayyad_square.py"
echo When the map is done, press Alt+P to play.
goto :end

:no_project
echo [!] UmayyadStrike.uproject not found next to Setup.bat.
echo     Extract the ZIP fully first, then run Setup.bat from inside the project folder.
echo no project >> "%LOG%"
goto :end

:no_engine
echo [!] Unreal Engine 5.8 not found.
echo     Run it like this (with your real path):
echo     Setup.bat "D:\Program Files\Epic Games\UE_5.8"
echo no engine >> "%LOG%"
goto :end

:no_vs
echo [!] Visual Studio 2022 with C++ is not installed - it is required to compile the game.
echo     Install "Visual Studio 2022 Community" and tick:
echo       - Game development with C++   (include "Unreal Engine installer" + Windows SDK)
echo       - Desktop development with C++
echo     Then run Setup.bat again.
echo no visual studio >> "%LOG%"
goto :end

:build_failed
echo [!] Build FAILED. First errors:
echo ------------------------------------------------------------
findstr /i /c:"error" "Saved\build_log.txt"
echo ------------------------------------------------------------
echo Full log: Saved\build_log.txt  - send it to Claude.
echo build failed >> "%LOG%"
goto :end

:end
echo.
pause
endlocal
