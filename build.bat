@echo off
call "D:\Program Files (x86)\Microsoft Visual Studio 8\VC\bin\vcvars32.bat"
cd /d "d:\project\ies\cad\exploration"
vcbuild /time exploration2005.vcproj Debug > build_log7.txt 2>&1
echo Exit code: %ERRORLEVEL%
dir Debug2005\*.arx
