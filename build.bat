@echo off
rem ==========================================
rem   Secondary Motion Tool Build Script
rem   (EIEM-compatible eiem.dll, proxy loaders)
rem ==========================================
setlocal enabledelayedexpansion

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "vcvars="
set "install_path="

if exist "%vswhere%" (
    for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "install_path=%%i"
    )
)

if defined install_path (
    set "vcvars=!install_path!\VC\Auxiliary\Build\vcvars64.bat"
)

if defined vcvars if exist "!vcvars!" (
    echo [INFO] Found MSVC at: "!vcvars!"
    call "!vcvars!" >nul
)

where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] MSVC environment not found.
    exit /b 1
)

if not exist bin mkdir bin

echo [1/4] Compiling version resource ...
rc /nologo /fo bin\version.res src\version.rc

echo [2/4] Building eiem.dll ...
cl /nologo /utf-8 /O2 /MD /LD /EHsc /std:c++17 ^
    /Ideps\minhook_lib\include ^
    src\eiem.cpp ^
    bin\version.res ^
    deps\minhook_lib\lib\libMinHook.x64.lib ^
    user32.lib ^
    gdi32.lib ^
    winmm.lib ^
    comdlg32.lib ^
    d3d11.lib ^
    dxgi.lib ^
    dwmapi.lib ^
    ole32.lib ^
    winhttp.lib ^
    /Fe"bin\eiem.dll" ^
    /link /DLL

if %errorlevel% neq 0 (
    echo [ERROR] eiem.dll build failed!
    exit /b 1
)
echo [OK] eiem.dll built successfully

echo [3/4] Building d3dcompiler_47.dll (proxy loader) ...
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
    src\proxy_d3dcompiler.cpp ^
    /Fe"bin\d3dcompiler_47.dll" ^
    /link /DLL
if %errorlevel% neq 0 (
    echo [ERROR] d3dcompiler_47.dll build failed!
    exit /b 1
)

echo [4/4] Building vulkan-1.dll (vulkan proxy loader) ...
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
    src\proxy_vulkan_full.cpp ^
    /Fe"bin\vulkan-1.dll" ^
    /link /DLL
if %errorlevel% neq 0 (
    echo [ERROR] vulkan-1.dll build failed!
    exit /b 1
)

del /q eiem.obj 2>nul
del /q proxy_d3dcompiler.obj 2>nul
del /q proxy_vulkan_full.obj 2>nul
del /q bin\eiem.exp 2>nul
del /q bin\eiem.lib 2>nul
del /q bin\d3dcompiler_47.exp 2>nul
del /q bin\d3dcompiler_47.lib 2>nul
del /q bin\vulkan-1.exp 2>nul
del /q bin\vulkan-1.lib 2>nul

echo ==========================================
echo   Build Complete!
echo ==========================================
echo Output files in bin\:
echo   - eiem.dll               (secondary motion plugin)
echo   - d3dcompiler_47.dll     (DX proxy loader)
echo   - vulkan-1.dll           (Vulkan proxy loader)
