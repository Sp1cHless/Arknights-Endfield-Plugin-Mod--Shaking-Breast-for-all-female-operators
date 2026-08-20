@echo off
rem ============================================================
rem assemble_one.bat - assemble one language package from the
rem shared publish output (dist\staging_app) and zip it.
rem Usage: assemble_one.bat <LANG> <VERSION>
rem   LANG = EN or ZH (zip filename + default_lang.txt inside)
rem ============================================================
setlocal

set "LANG=%~1"
set "VER=%~2"
set "ROOT=%~dp0"
set "DIST=%ROOT%dist"
set "APP=%DIST%\staging_app"
set "STAGE=%DIST%\staging_%LANG%\SecondaryMotion"

if exist "%DIST%\staging_%LANG%" rmdir /s /q "%DIST%\staging_%LANG%"
mkdir "%STAGE%"

echo   Copying Manager build (%LANG%) ...
copy /y "%APP%\*" "%STAGE%\" >nul

echo   Assembling %LANG% package ...
mkdir "%STAGE%\plugin"
mkdir "%STAGE%\data"
mkdir "%STAGE%\presets"
mkdir "%STAGE%\runtime"
copy /y "%ROOT%bin\sbm.dll" "%STAGE%\plugin\" >nul
copy /y "%ROOT%bin\d3dcompiler_47.dll" "%STAGE%\plugin\" >nul
copy /y "%ROOT%bin\vulkan-1.dll" "%STAGE%\plugin\" >nul
copy /y "%ROOT%SecondaryMotion\data\characters.default.json" "%STAGE%\data\characters.default.template.json" >nul
copy /y "%ROOT%SecondaryMotion\presets\Default.json" "%STAGE%\presets\Default.template.json" >nul
if exist "%ROOT%SecondaryMotion\presets\User.json" copy /y "%ROOT%SecondaryMotion\presets\User.json" "%STAGE%\presets\User.template.json" >nul
copy /y "%ROOT%SecondaryMotion\runtime\config.json" "%STAGE%\runtime\" >nul
copy /y "%ROOT%USER_GUIDE_EN.txt" "%STAGE%\" >nul
copy /y "%ROOT%README.md" "%STAGE%\" >nul
rem language default for this package
if /i "%LANG%"=="EN" (
    echo en-US> "%STAGE%\default_lang.txt"
) else (
    echo zh-CN> "%STAGE%\default_lang.txt"
)
rem ZH package: localize character display names in the DB copy and
rem add the Chinese user guide (zh_names.py handles both; stage arg = 2nd)
if /i "%LANG%"=="ZH" (
    python "%ROOT%zh_names.py" "%STAGE%\data\characters.default.template.json" "%STAGE%"
)
rem drop dev-only artifacts that publish may have produced
del /q "%STAGE%\*.pdb" 2>nul
del /q "%STAGE%\settings.json" 2>nul

echo   Zipping %LANG% ...
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%' -DestinationPath '%DIST%\ShakingBreastManager-%VER%-%LANG%-win-x64.zip' -Force"
if errorlevel 1 ( echo [ERROR] zip %LANG% failed & exit /b 1 )

echo   Package: %DIST%\ShakingBreastManager-%VER%-%LANG%-win-x64.zip
