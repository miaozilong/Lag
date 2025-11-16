@echo off
REM 检查是否以管理员权限运行
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo This script requires Administrator privileges.
    echo Restarting with elevated privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================
echo Build and Deploy Lag Plugin to TrafficMonitor
echo ========================================

REM 设置路径
set "SOURCE_DLL=Bin\x64\Release\Lag.dll"
set "TARGET_DIR=C:\Users\miaozilong\AppData\Local\TrafficMonitor\plugins"
set "TRAFFICMONITOR_EXE=C:\Users\miaozilong\AppData\Local\TrafficMonitor\TrafficMonitor.exe"

REM 自动检测 MSBuild 路径（优先使用 vswhere）
echo Detecting MSBuild path...
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul') do set "MSBUILD_PATH=%%i"
)

REM 如果 vswhere 未找到，尝试常见路径
if not defined MSBUILD_PATH (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )
)

echo Step 1: Cleaning Bin directory...
if exist "Bin" (
    echo Cleaning Bin directory...
    rmdir /s /q "Bin" 2>nul
    echo ✓ Bin directory cleaned
) else (
    echo Bin directory does not exist, skipping cleanup
)

echo.
echo Step 2: Building Lag.dll...

REM 检查MSBuild是否存在
if not defined MSBUILD_PATH (
    echo Error: MSBuild not found
    echo Please install Visual Studio with MSBuild component
    pause
    exit /b 1
)

if not exist "%MSBUILD_PATH%" (
    echo Error: MSBuild not found at %MSBUILD_PATH%
    echo Please install Visual Studio with MSBuild component
    pause
    exit /b 1
)

echo Using MSBuild from: %MSBUILD_PATH%
echo Current directory: %CD%
echo Solution file: %~dp0TrafficMonitorPlugins.sln

REM 切换到脚本所在目录
cd /d "%~dp0"

echo Changed to directory: %CD%
"%MSBUILD_PATH%" "%~dp0TrafficMonitorPlugins.sln" /p:Configuration=Release /p:Platform=x64 /t:Lag /v:minimal

if %errorlevel% equ 0 (
    echo ✓ Build successful!
) else (
    echo Error: Build failed with error code %errorlevel%
    pause
    exit /b 1
)

echo.
echo Step 3: Checking if TrafficMonitor is running...
tasklist /FI "IMAGENAME eq TrafficMonitor.exe" 2>NUL | find /I /N "TrafficMonitor.exe">NUL
if %errorlevel% equ 0 (
    echo TrafficMonitor is running, stopping it...
    taskkill /F /IM TrafficMonitor.exe
    timeout /t 3 /nobreak >nul
    
    REM 再次检查是否还在运行
    tasklist /FI "IMAGENAME eq TrafficMonitor.exe" 2>NUL | find /I /N "TrafficMonitor.exe">NUL
    if %errorlevel% equ 0 (
        echo Warning: TrafficMonitor is still running, trying alternative method...
        powershell -Command "Get-Process -Name 'TrafficMonitor' -ErrorAction SilentlyContinue | Stop-Process -Force"
        timeout /t 2 /nobreak >nul
    )
    echo TrafficMonitor stopped
) else (
    echo TrafficMonitor is not running
)

echo.
echo Step 4: Checking target directory...
if not exist "%TARGET_DIR%" (
    echo Error: Target directory does not exist: %TARGET_DIR%
    echo Please check if TrafficMonitor is installed correctly
    pause
    exit /b 1
)

echo Target directory exists: %TARGET_DIR%

echo.
echo Step 5: Backing up existing Lag.dll (if exists)...
if exist "%TARGET_DIR%\Lag.dll" (
    copy "%TARGET_DIR%\Lag.dll" "%TARGET_DIR%\Lag.dll.backup" >nul 2>&1
    if %errorlevel% equ 0 (
        echo Backup created: Lag.dll.backup
    ) else (
        echo Warning: Failed to create backup
    )
)

echo.
echo Step 6: Copying new Lag.dll...
if not exist "%SOURCE_DLL%" (
    echo Error: Source DLL not found: %SOURCE_DLL%
    echo Please build the project first
    pause
    exit /b 1
)

copy "%SOURCE_DLL%" "%TARGET_DIR%\" >nul
if %errorlevel% equ 0 (
    echo ✓ Lag.dll copied successfully
) else (
    echo Error: Failed to copy Lag.dll
    echo You may need to run this script as Administrator
    pause
    exit /b 1
)

echo.
echo Step 7: Starting TrafficMonitor...
if exist "%TRAFFICMONITOR_EXE%" (
    echo Starting TrafficMonitor...
    start "" "%TRAFFICMONITOR_EXE%"
    if %errorlevel% equ 0 (
        echo ✓ TrafficMonitor started successfully
    ) else (
        echo Error: Failed to start TrafficMonitor
    )
) else (
    echo Error: TrafficMonitor.exe not found at %TRAFFICMONITOR_EXE%
    echo Please check the installation path
)

echo.
echo ========================================
echo Deployment completed!
echo ========================================
echo.
echo Summary:
echo - Lag.dll has been deployed to: %TARGET_DIR%
echo - TrafficMonitor has been restarted
echo - Backup of old DLL saved as: Lag.dll.backup
echo.
