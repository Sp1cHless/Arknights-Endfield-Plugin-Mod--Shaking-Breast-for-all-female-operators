@echo off
rem Deploy V2: eiem.dll + SecondaryMotion runtime data to the game folder.
rem USAGE: deploy.bat [name-tag]   (tag appended to the backup name)
rem NOTE: this file must stay pure ASCII (cmd parses .bat as GBK).
setlocal

set "GAMEROOT=E:\GAMU\Hypergryph Launcher\games\Endfield Game"
set "GAME=%GAMEROOT%\plugin"
set "TAG=%1"
if "%TAG%"=="" set "TAG=pre_v2"

if not exist "%GAME%" (
    echo [ERROR] Game plugin dir not found: %GAME%
    echo.
    pause
    exit /b 1
)
if not exist bin\eiem.dll (
    echo [ERROR] bin\eiem.dll missing - run run_build.bat first
    echo.
    pause
    exit /b 1
)

echo [1/4] Backing up current active DLL...
if exist "%GAME%\eiem.dll" (
    copy /y "%GAME%\eiem.dll" "%GAME%\eiem.dll.%TAG%" >nul
    echo   backup: %GAME%\eiem.dll.%TAG%
) else (
    echo   no active eiem.dll - fresh install
)

echo [2/4] Copying new build...
copy /y bin\eiem.dll "%GAME%\eiem.dll" >nul
if exist bin\d3dcompiler_47.dll copy /y bin\d3dcompiler_47.dll "%GAMEROOT%\d3dcompiler_47.dll" >nul
if exist bin\vulkan-1.dll copy /y bin\vulkan-1.dll "%GAMEROOT%\vulkan-1.dll" >nul

echo [3/4] Initializing SecondaryMotion data...
if not exist "%GAMEROOT%\SecondaryMotion\data" mkdir "%GAMEROOT%\SecondaryMotion\data"
if not exist "%GAMEROOT%\SecondaryMotion\presets" mkdir "%GAMEROOT%\SecondaryMotion\presets"
if not exist "%GAMEROOT%\SecondaryMotion\runtime" mkdir "%GAMEROOT%\SecondaryMotion\runtime"
if not exist "%GAMEROOT%\SecondaryMotion\developer" mkdir "%GAMEROOT%\SecondaryMotion\developer"
if not exist "%GAMEROOT%\SecondaryMotion\logs" mkdir "%GAMEROOT%\SecondaryMotion\logs"
rem NOTE: the Manager exe is NOT deployed here. It lives in the project
rem folder (SecondaryMotion\SecondaryMotion.Manager.exe) and reads/writes
rem the game data dir via its settings.json.
rem DATA FILES are ONLY initialized when missing (if not exist) - the
rem game dir data is the user's working copy (Manager edits it); deploying
rem must never clobber it.
if not exist "%GAMEROOT%\SecondaryMotion\data\characters.default.json" copy /y SecondaryMotion\data\characters.default.json "%GAMEROOT%\SecondaryMotion\data\characters.default.json" >nul
if not exist "%GAMEROOT%\SecondaryMotion\presets\Default.json" copy /y SecondaryMotion\presets\Default.json "%GAMEROOT%\SecondaryMotion\presets\Default.json" >nul
if not exist "%GAMEROOT%\SecondaryMotion\presets\User.json" copy /y SecondaryMotion\presets\User.json "%GAMEROOT%\SecondaryMotion\presets\User.json" >nul
if not exist "%GAMEROOT%\SecondaryMotion\developer\diagnostics.json" copy /y SecondaryMotion\developer\diagnostics.json "%GAMEROOT%\SecondaryMotion\developer\diagnostics.json" >nul
if not exist "%GAMEROOT%\SecondaryMotion\runtime\config.json" (
    copy /y SecondaryMotion\runtime\config.json "%GAMEROOT%\SecondaryMotion\runtime\config.json" >nul
)

echo [4/4] Hashing...
echo.
echo ==========================================
echo   Deployed. Verify hashes:
echo ==========================================
certutil -hashfile bin\eiem.dll SHA256 | findstr /v "hash"
certutil -hashfile "%GAME%\eiem.dll" SHA256 | findstr /v "hash"
echo.
echo REMEMBER: game must be EXITED before deploying (eiem.dll is locked).
echo.
pause
