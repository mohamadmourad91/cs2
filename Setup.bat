@echo off
setlocal
rem ============================================================
rem  Umayyad Strike - one-click build + map generation (Windows)
rem  Usage:  Setup.bat                      (auto-detects UE 5.5-5.7)
rem          Setup.bat "D:\Epic\UE_5.5"      (custom engine path)
rem ============================================================
set "PROJ=%~dp0UmayyadStrike.uproject"
set "UE=%~1"
if "%UE%"=="" for %%V in (5.7 5.6 5.5) do if "%UE%"=="" if exist "C:\Program Files\Epic Games\UE_%%V\Engine" set "UE=C:\Program Files\Epic Games\UE_%%V"
if "%UE%"=="" (
  echo [!] Unreal Engine not found. Run:  Setup.bat "path\to\UE_5.x"
  exit /b 1
)
echo Engine: %UE%
if not exist "%~dp0Saved" mkdir "%~dp0Saved"

echo.
echo [1/2] Compiling UmayyadStrikeEditor (log: Saved\build_log.txt) ...
call "%UE%\Engine\Build\BatchFiles\Build.bat" UmayyadStrikeEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload > "%~dp0Saved\build_log.txt" 2>&1
if errorlevel 1 (
  echo [!] Build FAILED. Errors:
  findstr /i /c:"error" "%~dp0Saved\build_log.txt"
  echo.
  echo Send Saved\build_log.txt to Claude to get the fix.
  exit /b 1
)
echo Build OK.

echo.
echo [2/2] Opening the editor and building Umayyad Square ...
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -ExecutePythonScript="%~dp0Content\Python\build_umayyad_square.py"
echo When the map finishes building, press Alt+P to play.
endlocal
