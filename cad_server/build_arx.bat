@echo off
REM ============================================================
REM  KsExploration.arx 编译脚本（2007 / Debug）
REM  用法: 在装有 VS2005 + ObjectARX 2007 SDK 的编译机上双击或命令行执行
REM  产物: D:\project\ies\cad\exploration\Debug\KsExploration.arx
REM ============================================================
setlocal
cd /d D:\project\ies\cad\exploration

REM --- 按本机实际 VS 安装路径修改下面这行 ---
set DEVENV="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com"

echo [build] %~dp0exploration.sln  Debug^|Win32
%DEVENV% exploration.sln /Build "Debug|Win32" /Project exploration
if errorlevel 1 (
    echo [build] FAILED exit=%errorlevel%
    exit /b 1
)

echo [build] OK
dir /b KsExploration.arx
endlocal
