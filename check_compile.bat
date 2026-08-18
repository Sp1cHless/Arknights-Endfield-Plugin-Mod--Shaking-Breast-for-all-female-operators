@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\Project\EndfieldBreastMotion
cl /nologo /utf-8 /O2 /MD /LD /EHsc /std:c++17 /Ideps\minhook_lib\include /c src\sbm.cpp /Fo:build_check.obj
echo CHECK_EXIT=%errorlevel%
