@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\Project\EndfieldBreastMotion
call build.bat > build_log.txt 2>&1
type build_log.txt
