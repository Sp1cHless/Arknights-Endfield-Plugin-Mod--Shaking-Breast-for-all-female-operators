@echo off
rem verify.bat - V2 automated verification. Run from project root.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\Project\EndfieldBreastMotion

echo === [1/3] Unit tests ===
cl /nologo /utf-8 /O2 /EHsc /std:c++17 verify_tests.cpp /Fe:verify_tests.exe > verify_build.log 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] verify_tests.cpp build failed
    type verify_build.log
    exit /b 1
)
verify_tests.exe
if %errorlevel% neq 0 (
    echo [VERIFY] unit tests FAILED
    exit /b 1
)

echo.
echo === [2/3] Forbidden hardcode scan (strict reflection) ===
set "VIOL=0"
findstr /s /n /c:"+ 0x" src\*.h src\*.cpp > nul && echo   VIOLATION: fixed offset addition in code && set "VIOL=1"
findstr /s /n /c:"gaMod +" src\*.h src\*.cpp > nul && echo   VIOLATION: RVA addition in code && set "VIOL=1"
findstr /s /n /c:"fallback 0x" src\*.h src\*.cpp > nul && echo   VIOLATION: fixed offset fallback && set "VIOL=1"
if "%VIOL%"=="0" (
    echo   PASS: no forbidden hardcode
) else (
    echo   [VERIFY] forbidden hardcode scan FAILED
    exit /b 1
)

echo.
echo === [3/3] JSON syntax sanity ===
python -c "import json; [json.load(open(f,encoding='utf-8')) for f in ['SecondaryMotion/data/characters.default.json','SecondaryMotion/presets/Default.json','SecondaryMotion/presets/User.json','SecondaryMotion/runtime/config.json']]; print('  PASS: all JSON valid')" 2>nul
if %errorlevel% neq 0 (
    echo   [VERIFY] JSON syntax check FAILED
    exit /b 1
)

echo.
echo ==========================================
echo   VERIFICATION: PASS
echo ==========================================
